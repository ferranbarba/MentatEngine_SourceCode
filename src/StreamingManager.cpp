#include "../include/MentatEngine/StreamingManager.hpp"
#include "../include/MentatEngine/ImGuiManager.hpp"
#include "../include/MentatEngine/ECSManager.hpp"
#include "../include/MentatEngine/CameraComponent.hpp"
#include "../include/MentatEngine/TransformComponent.hpp"
#include "../include/MentatEngine/OpenGL/RenderizableComponent.hpp"
#include "../include/MentatEngine/OpenGL/glMemoryTracker.hpp"
#include <cmath>
#include <algorithm>
#include <filesystem>

namespace ME {
    unsigned int StreamingManager::budget_mode_;
    OctreeNode StreamingManager::root_node_;
    std::unordered_map<unsigned long, EntityState> StreamingManager::current_entities_;
    std::unordered_map<std::string, float> StreamingManager::min_mesh_distances_;
    std::vector<float> StreamingManager::lod_thresholds_;
    std::vector<std::pair<unsigned long, ME::RenderizableComponent*>> entities_ordered_by_metric;
    unsigned int texture_memory_budget_bytes, mesh_memory_budget_bytes;
    size_t current_frame_texture_size_bytes, current_frame_mesh_size_bytes;
    Frustum frustum;

    namespace {
        constexpr unsigned int kBytesPerMiB = 1024U * 1024U;
        constexpr unsigned int kUltraMeshBudgetMiB = 1024U;
        constexpr unsigned int kUltraTextureBudgetMiB = 2048U;
        constexpr unsigned int kHighMeshBudgetMiB = 512U;
        constexpr unsigned int kHighTextureBudgetMiB = 1024U;
        constexpr unsigned int kMediumMeshBudgetMiB = 256U;
        constexpr unsigned int kMediumTextureBudgetMiB = 512U;
        constexpr unsigned int kLowMeshBudgetMiB = 128U;
        constexpr unsigned int kLowTextureBudgetMiB = 256U;
        constexpr unsigned int kUltraLowMeshBudgetMiB = 16U;
        constexpr unsigned int kUltraLowTextureBudgetMiB = 32U;
    }

    StreamingManager::StreamingManager() {}

    StreamingManager::~StreamingManager() {}

    void StreamingManager::Start() {
        // Definir lmites iniciales del mundo (AABB)
        root_node_.min_bound_.x_ = -100000.0f;
        root_node_.min_bound_.y_ = -100000.0f;
        root_node_.min_bound_.z_ = -100000.0f;

        root_node_.max_bound_.x_ = 100000.0f;
        root_node_.max_bound_.y_ = 100000.0f;
        root_node_.max_bound_.z_ = 100000.0f;

        budget_mode_ = 0;
        UpdateGlobalQuality(4);
        lod_thresholds_ = { 20.0f, 50.0f, 100.0f, 250.0f, 500.0f };
    }

    void StreamingManager::UpdateGlobalQuality(int quality) {
        switch (quality) {
            case 0:
                mesh_memory_budget_bytes = kUltraMeshBudgetMiB * kBytesPerMiB;
                texture_memory_budget_bytes = kUltraTextureBudgetMiB * kBytesPerMiB;
                break;
            case 1:
                mesh_memory_budget_bytes = kHighMeshBudgetMiB * kBytesPerMiB;
                texture_memory_budget_bytes = kHighTextureBudgetMiB * kBytesPerMiB;
                break;
            case 2:
                mesh_memory_budget_bytes = kMediumMeshBudgetMiB * kBytesPerMiB;
                texture_memory_budget_bytes = kMediumTextureBudgetMiB * kBytesPerMiB;
                break;
            case 3:
                mesh_memory_budget_bytes = kLowMeshBudgetMiB * kBytesPerMiB;
                texture_memory_budget_bytes = kLowTextureBudgetMiB * kBytesPerMiB;
                break;
            case 4:
                mesh_memory_budget_bytes = kUltraLowMeshBudgetMiB * kBytesPerMiB;
                texture_memory_budget_bytes = kUltraLowTextureBudgetMiB * kBytesPerMiB;
                break;
        }

        ME::glMemoryTracker::budget_buffers_ = mesh_memory_budget_bytes;
        ME::glMemoryTracker::budget_textures_ = texture_memory_budget_bytes;
    }

    void StreamingManager::RegisterEntity(unsigned long entityID) {
        auto* tc = ME::ECS::GetComponent<TransformComponent>(entityID);

        if (!tc) {
            printf("[SteamingManager] Trying to register an entity without transform component\n", entityID);
            return;
        }

        EntityState state;
        state.max_lods_available_ = 0;
        state.current_lod_ = 0;
        state.max_mipmaps_available_ = 0;
        state.current_mipmap_ = 0;
        current_entities_[entityID] = state;

        RecursiveInsertEntity(&root_node_, entityID, tc->Position().x_, tc->Position().y_, tc->Position().z_);
    }

    void StreamingManager::UpdateEntityMesh(unsigned long entityID) {
        auto it = current_entities_.find(entityID);
        if (it != current_entities_.end()) {
            if (auto* rc = ME::ECS::GetComponent<RenderizableComponent>(entityID)) {
                if (rc->GetMesh()) {
                    it->second.max_lods_available_ = rc->GetMesh()->GetLODQuantity();
                    it->second.current_lod_ = rc->GetMesh()->GetLOD();
                }
            }
        }
    }

    void StreamingManager::UpdateEntityTexture(unsigned long entityID) {
        auto it = current_entities_.find(entityID);
        if (it != current_entities_.end()) {
            if (auto* rc = ME::ECS::GetComponent<RenderizableComponent>(entityID)) {
                if (rc->GetTexture()) {
                    it->second.max_mipmaps_available_ = rc->GetTexture()->GetMipMapsQuantity();
                    it->second.current_mipmap_ = rc->GetTexture()->GetMipMap();
                }
            }
        }
    }

    void StreamingManager::DeleteEntity(unsigned long entityID) {
        current_entities_.erase(entityID);
        RecursiveRemoveEntity(&root_node_, entityID);
    }

    void StreamingManager::ReinsertEntity(unsigned long entityID) {
        auto it = current_entities_.find(entityID);
        if (it == current_entities_.end()) return;

        auto* tc = ME::ECS::GetComponent<TransformComponent>(entityID);
        auto* rc = ME::ECS::GetComponent<RenderizableComponent>(entityID);
        if (!tc || !rc) return;

        Vec3 pos = tc->Position();
        OctreeNode* node = it->second.current_node_;

        // Si la entidad se ha salido de los lmites de su nodo actual
        if (node && !node->Contains(pos.x_, pos.y_, pos.z_)) {
            RecursiveRemoveEntity(&root_node_, entityID);
            RecursiveInsertEntity(&root_node_, entityID, pos.x_, pos.y_, pos.z_);
        }
    }

    void StreamingManager::Update() {
        // Check for updates in the nodes
        for (auto& [id, state] : current_entities_) {
            ReinsertEntity(id);
        }

        auto& cameraList = ME::ECS::GetComponentList<ME::CameraComponent>();
        if (cameraList.empty()) return;
        auto& cam = cameraList.at(0).second;

        // Clear the previous minimum distances and metrics lists
        min_mesh_distances_.clear();
        entities_ordered_by_metric.clear();

        // Update frustrum
        Mat4 viewProj = cam.proj;
        viewProj.multiply(cam.view);
        frustum.UpdateFrustum(viewProj);

        // Fill the new distances and update the metrics based on distance and volume
        AnalyzeNode(&root_node_, cam.position.x_, cam.position.y_, cam.position.z_);

        // Uses the octree to check if the entities are inside or outside the view
        FrustumCulling(&root_node_);

        // Fill the current use of memory for this frame
        current_frame_mesh_size_bytes = ME::glMemoryTracker::GetBufferMemory();
        current_frame_texture_size_bytes = ME::glMemoryTracker::GetTextureMemory();

        // Order by higher metric value
        std::sort(entities_ordered_by_metric.begin(), entities_ordered_by_metric.end(), [&](const auto& a, const auto& b) {
            return current_entities_[a.first].current_metric_value_ < current_entities_[b.first].current_metric_value_;
        });

        // Applies the Memory Budgets
        UpdateBaseOnBudget(&root_node_, cam.position.x_, cam.position.y_, cam.position.z_);
    }

    void StreamingManager::AnalyzeNode(OctreeNode* node, float camX, float camY, float camZ) {
        if (node->is_leaf_) {
            for (unsigned long id : node->entity_ids_) {
                auto* rc = ME::ECS::GetComponent<RenderizableComponent>(id);
                auto* tc = ME::ECS::GetComponent<TransformComponent>(id);
                if (!rc || !rc->GetMesh() || !tc) continue;

                Frustum::Visibility visibility = frustum.CheckBox(rc->GetMesh()->GetMinBounds(), rc->GetMesh()->GetMaxBounds());
                if (visibility == Frustum::Visibility::INSIDE || visibility == Frustum::Visibility::INTERSECT) {
                    if (!rc->Visible()) rc->SetVisibility(true);
                }
                else if (rc->Visible()) {
                    rc->SetVisibility(false);
                }

                // PRECACHE THE RENDERIZABLE COMPONENT
                entities_ordered_by_metric.push_back(std::make_pair(id, rc));

                float dist = std::sqrt(std::pow(camX - tc->Position().x_, 2) +
                    std::pow(camY - tc->Position().y_, 2) +
                    std::pow(camZ - tc->Position().z_, 2));

                std::shared_ptr<glMesh> mesh = rc->GetMesh();
                std::string path = rc->GetMesh()->GetMeshPath();
                if (min_mesh_distances_.find(path) == min_mesh_distances_.end() || dist < min_mesh_distances_[path]) {
                    min_mesh_distances_[path] = dist;
                }

                Vec3 maxb = mesh->GetMaxBounds();
                Vec3 minb = mesh->GetMinBounds();
                Vec3 scale = tc->Scale();

                float volume = (maxb.x_ - minb.x_) * (maxb.y_ - minb.y_) * (maxb.z_ - minb.z_);
                float worldVolume = powf(volume * (scale.x_ * scale.y_ * scale.z_), 1.0f/3.0f);

                auto it = current_entities_.find(id);
                if (it != current_entities_.end()) {
                    it->second.current_metric_value_ = (dist != 0.0f) ? (worldVolume / dist) * 150.0f : 0.0f;
                }
            }
        }
        else {
            for (int i = 0; i < 8; ++i) {
                if (node->children_[i]) AnalyzeNode(node->children_[i].get(), camX, camY, camZ);
            }
        }
    }

    void StreamingManager::FrustumCulling(OctreeNode* node) {

        Frustum::Visibility nodeVis = frustum.CheckBox(node->min_bound_, node->max_bound_);

        if (nodeVis == Frustum::OUTSIDE) {
            SetNodeVisibility(node, false);
            return;
        }

        if (nodeVis == Frustum::INSIDE) {
            SetNodeVisibility(node, true);
            return;
        }

        // INTERSECT
        if (node->is_leaf_) {
            for (unsigned long id : node->entity_ids_) {
                auto* rc = ME::ECS::GetComponent<RenderizableComponent>(id);
                auto* tc = ME::ECS::GetComponent<TransformComponent>(id);
                if (!rc || !tc || !rc->GetMesh()) continue;

                auto bb = rc->GetBoundingBox(tc->WorldMatrix());
                Vec3 world_min_ = bb.first;
                Vec3 world_max_ = bb.second;

                bool isVisible = (frustum.CheckBox(world_min_, world_max_) != Frustum::OUTSIDE);
                rc->SetVisibility(isVisible);
            }
        }
        else {
            for (int i = 0; i < 8; ++i) {
                if (node->children_[i]) {
                    FrustumCulling(node->children_[i].get());
                }
            }
        }
    }

    void StreamingManager::UpdateBaseOnBudget(OctreeNode* node, float camX, float camY, float camZ) {
        switch (budget_mode_) {
            case 0:
                {
                // =================================================================
                // PREDICTION BASED ON CURRENT LOADING MESHES/TEXTURES BASED ON ACTUAL MEMORY
                // =================================================================
                for (const auto& [id, rc] : entities_ordered_by_metric) {
                    auto& state = current_entities_[id];

                    if (rc->GetMesh()->IsLODPending()) {
                        size_t currentMeshMem = ME::glMemoryTracker::GetEntityBufferMemory(id);
                        if (rc->GetMesh()->GetLOD() > state.current_lod_) {
                            size_t estimatedNextMem = currentMeshMem * 4;
                            current_frame_mesh_size_bytes += (estimatedNextMem - currentMeshMem);
                        }
                        else if (rc->GetMesh()->GetLOD() < state.current_lod_) {
                            size_t savedMem = currentMeshMem - (currentMeshMem / 4);
                            if (current_frame_mesh_size_bytes > savedMem) current_frame_mesh_size_bytes -= savedMem;
                        }
                    }

                    if (rc->GetTexture()->IsMipMapPending()) {
                        size_t currentTexMem = ME::glMemoryTracker::GetEntityTextureMemory(id);
                        if (rc->GetTexture()->GetMipMap() > state.current_mipmap_) {
                            size_t estimatedNextMem = currentTexMem * 4;
                            current_frame_texture_size_bytes += (estimatedNextMem - currentTexMem);
                        }
                        else if (rc->GetTexture()->GetMipMap() < state.current_mipmap_) {
                            size_t savedMem = currentTexMem - (currentTexMem / 4);
                            if (current_frame_texture_size_bytes > savedMem) current_frame_texture_size_bytes -= savedMem;
                        }
                    }
                }

                // =================================================================
                // QUALITY DOWNGRADE BASED ON METRIC FROM LOWER TO HIGHER
                // =================================================================
                for (auto it = entities_ordered_by_metric.rbegin(); it != entities_ordered_by_metric.rend(); ++it) {
                    unsigned long id = it->first;
                    auto* rc = it->second;
                    if (!rc || rc->GetMesh()->IsLODPending() || rc->GetTexture()->IsMipMapPending()) continue;

                    auto& state = current_entities_[id];

                    if (current_frame_mesh_size_bytes > mesh_memory_budget_bytes) {
                        int lowerLOD = std::min(state.current_lod_ + 1, state.max_lods_available_);
                        if (lowerLOD != state.current_lod_) {
                            size_t currentMeshMem = ME::glMemoryTracker::GetEntityBufferMemory(id);
                            size_t savedMem = currentMeshMem - (currentMeshMem / 4);

                            state.current_lod_ = lowerLOD;
                            rc->GetMesh()->SetLOD(lowerLOD);
                            ME::MeshLoader::AddMeshToPending(rc->GetMesh()->GetMeshPath(), id);

                            if (current_frame_mesh_size_bytes > savedMem) current_frame_mesh_size_bytes -= savedMem;
                        }
                    }

                    if (current_frame_texture_size_bytes > texture_memory_budget_bytes) {
                        int lowerMipMap = std::min(state.current_mipmap_ + 1, state.max_mipmaps_available_);
                        if (lowerMipMap != state.current_mipmap_) {
                            size_t currentTexMem = ME::glMemoryTracker::GetEntityTextureMemory(id);
                            size_t savedMem = currentTexMem - (currentTexMem / 4);

                            state.current_mipmap_ = lowerMipMap;
                            rc->GetTexture()->SetMipMap(lowerMipMap);
                            ME::TextureLoader::AddTextureToPending(rc->GetTexture()->GetTexturePath(), id);

                            if (current_frame_texture_size_bytes > savedMem) current_frame_texture_size_bytes -= savedMem;
                        }
                    }
                }

                // =================================================================
                // QUALITY UPGRADE BASED ON METRIC FROM HIGHER TO LOWER (WITH HYSTERESIS THRESHOLD)
                // =================================================================
                const double HYSTERESIS = 0.90;
                size_t safe_mesh_budget = mesh_memory_budget_bytes * HYSTERESIS;
                size_t safe_texture_budget = texture_memory_budget_bytes * HYSTERESIS;

                int uploads_count = 0;
                const int MAX_UPLOADS_PER_FRAME = 1;

                for (const auto& [id, rc] : entities_ordered_by_metric) {
                    if (uploads_count >= MAX_UPLOADS_PER_FRAME) break;

                    auto* rc = ME::ECS::GetComponent<RenderizableComponent>(id);
                    if (!rc || rc->GetMesh()->IsLODPending() || rc->GetTexture()->IsMipMapPending()) continue;

                    auto& state = current_entities_[id];
                    float metric = state.current_metric_value_;

                    size_t currentMeshMem = ME::glMemoryTracker::GetEntityBufferMemory(id);

                    int desiredMetric = GetTargetQualityByMetric(
                        metric,
                        state.current_lod_,
                        state.max_lods_available_,
                        currentMeshMem,
                        current_frame_mesh_size_bytes,
                        mesh_memory_budget_bytes
                    );

                    if (state.current_lod_ > desiredMetric) {
                        size_t memoryIncrease = (currentMeshMem * 4) - currentMeshMem;

                        if (current_frame_mesh_size_bytes + memoryIncrease > safe_mesh_budget) {
                            for (auto it = entities_ordered_by_metric.rbegin(); it != entities_ordered_by_metric.rend(); ++it) {
                                unsigned long victim_id = it->first;
                                auto* victim_rc = it->second;

                                if (victim_id == id) break;

                                if (victim_rc->GetMesh()->IsLODPending()) continue;

                                auto& victim_state = current_entities_[victim_id];
                                int victim_lowerLOD = std::min(victim_state.current_lod_ + 1, victim_state.max_lods_available_);

                                if (victim_lowerLOD != victim_state.current_lod_) {
                                    size_t victim_mem = ME::glMemoryTracker::GetEntityBufferMemory(victim_id);
                                    size_t victim_saved = victim_mem - (victim_mem / 4);

                                    victim_state.current_lod_ = victim_lowerLOD;
                                    victim_rc->GetMesh()->SetLOD(victim_lowerLOD);
                                    ME::MeshLoader::AddMeshToPending(victim_rc->GetMesh()->GetMeshPath(), victim_id);

                                    if (current_frame_mesh_size_bytes > victim_saved) current_frame_mesh_size_bytes -= victim_saved;
                                }
                                if (current_frame_mesh_size_bytes + memoryIncrease <= safe_mesh_budget) break;
                            }
                        }

                        if (current_frame_mesh_size_bytes + memoryIncrease <= safe_mesh_budget) {
                            state.current_lod_ = state.current_lod_ - 1;
                            rc->GetMesh()->SetLOD(state.current_lod_);
                            ME::MeshLoader::AddMeshToPending(rc->GetMesh()->GetMeshPath(), id);
                            current_frame_mesh_size_bytes += memoryIncrease;
                            uploads_count++;
                            continue;
                        }
                    }

                    size_t currentTexMem = ME::glMemoryTracker::GetEntityTextureMemory(id);

                    if (state.current_mipmap_ > desiredMetric) {
                        size_t memoryIncrease = (currentTexMem * 4) - currentTexMem;

                        if (current_frame_texture_size_bytes + memoryIncrease > safe_texture_budget) {
                            for (auto it = entities_ordered_by_metric.rbegin(); it != entities_ordered_by_metric.rend(); ++it) {
                                unsigned long victim_id = it->first;
                                auto* victim_rc = it->second;

                                if (victim_id == id) break;
                                if (!victim_rc || victim_rc->GetTexture()->IsMipMapPending()) continue;

                                auto& victim_state = current_entities_[victim_id];
                                int victim_lowerMip = std::min(victim_state.current_mipmap_ + 1, victim_state.max_mipmaps_available_);

                                if (victim_lowerMip != victim_state.current_mipmap_) {
                                    size_t victim_mem = ME::glMemoryTracker::GetEntityTextureMemory(victim_id);
                                    size_t victim_saved = victim_mem - (victim_mem / 4);

                                    victim_state.current_mipmap_ = victim_lowerMip;
                                    victim_rc->GetTexture()->SetMipMap(victim_lowerMip);
                                    ME::TextureLoader::AddTextureToPending(rc->GetTexture()->GetTexturePath(), victim_id);

                                    if (current_frame_texture_size_bytes > victim_saved) current_frame_texture_size_bytes -= victim_saved;
                                }
                                if (current_frame_texture_size_bytes + memoryIncrease <= safe_texture_budget) break;
                            }
                        }

                        if (current_frame_texture_size_bytes + memoryIncrease <= safe_texture_budget) {
                            state.current_mipmap_ = state.current_mipmap_ - 1;
                            rc->GetTexture()->SetMipMap(state.current_mipmap_);
                            ME::TextureLoader::AddTextureToPending(rc->GetTexture()->GetTexturePath(), id);
                            current_frame_texture_size_bytes += memoryIncrease;
                            uploads_count++;
                        }
                    }
                }

                }
            break;

            case 1:
                if (node->is_leaf_) {
                    for (unsigned long id : node->entity_ids_) {
                        auto it = current_entities_.find(id);
                        if (it != current_entities_.end()) {
                            auto* rc = ME::ECS::GetComponent<RenderizableComponent>(id);
                            if (!rc || !rc->GetMesh()) continue;

                            std::string path = rc->GetMesh()->GetMeshPath();
                            float minDist = min_mesh_distances_[path];

                            ProcessLOD(id, it->second, minDist);
                            ProcessMipMap(id, it->second, minDist);
                        }
                    }
                }
                else {
                    for (int i = 0; i < 8; ++i) {
                        if (node->children_[i]) UpdateBaseOnBudget(node->children_[i].get(), camX, camY, camZ);
                    }
                }
            break;

            case 2:
                if (node->is_leaf_) {
                    for (unsigned long id : node->entity_ids_) {
                        auto it = current_entities_.find(id);
                        if (it != current_entities_.end()) {
                            if (it->second.current_lod_ != 0) {
                                auto* rc = ME::ECS::GetComponent<RenderizableComponent>(id);
                                if (!rc) continue;
                                rc->GetMesh()->SetLOD(0);
                                it->second.current_lod_ = 0;
                                ME::MeshLoader::AddMeshToPending(rc->GetMesh()->GetMeshPath(), id);
                            }

                            if (it->second.current_mipmap_ != 0) {
                                auto* rc = ME::ECS::GetComponent<RenderizableComponent>(id);
                                if (!rc) continue;
                                rc->GetTexture()->SetMipMap(0);
                                it->second.current_mipmap_ = 0;
                                ME::TextureLoader::AddTextureToPending(rc->GetTexture()->GetTexturePath(), id);
                            }
                        }
                    }
                }
                else {
                    for (int i = 0; i < 8; ++i) {
                        if (node->children_[i]) UpdateBaseOnBudget(node->children_[i].get(), camX, camY, camZ);
                    }
                }
            break;
        }
    }

    void StreamingManager::RecursiveInsertEntity(OctreeNode* node, unsigned long entityID, float x, float y, float z, int depth) {
        if (!node) return;
        constexpr int kMaxOctreeDepth = 8;

        if (node->is_leaf_) {
            if (node->entity_ids_.size() < node->kMaxEntities || depth >= kMaxOctreeDepth) {
                node->entity_ids_.push_back(entityID);
                current_entities_[entityID].current_node_ = node;
                return;
            }
            else {
                Subdivide(node);
            }
        }

        float p[3] = { x, y, z };
        bool placed = false;
        for (int i = 0; i < 8; ++i) {
            if (node->children_[i] && node->children_[i]->Contains(p[0], p[1], p[2])) {
                RecursiveInsertEntity(node->children_[i].get(), entityID, x, y, z, depth + 1);
                placed = true;
                break;
            }
        }

        if (!placed) {
            node->entity_ids_.push_back(entityID);
            current_entities_[entityID].current_node_ = node;
        }
    }

    void StreamingManager::RecursiveRemoveEntity(OctreeNode* node, unsigned long entityID) {
        if (node->is_leaf_) {
            auto it = std::find(node->entity_ids_.begin(), node->entity_ids_.end(), entityID);
            if (it != node->entity_ids_.end()) {
                node->entity_ids_.erase(it);
            }
        }
        else {
            for (int i = 0; i < 8; ++i) {
                if (node->children_[i]) {
                    RecursiveRemoveEntity(node->children_[i].get(), entityID);
                }
            }
        }
    }

    void StreamingManager::ProcessLOD(unsigned long id, EntityState& state, float distance) {
        auto* rc = ME::ECS::GetComponent<RenderizableComponent>(id);
        if (!rc || !rc->GetMesh()) return;

        if (state.max_lods_available_ == 0) state.max_lods_available_ = rc->GetMesh()->GetLODQuantity();

        int targetLOD = GetTargetNewIndex(distance, state.max_lods_available_);

        if (targetLOD != state.current_lod_) { // NECESITA CAMBIO DE MALLA
            if (rc->GetMesh()->IsLODPending()) { // YA SE EST CAMBIANDO
                state.current_lod_ = targetLOD;
                return;
            }

            rc->GetMesh()->SetLOD(targetLOD);
            state.current_lod_ = targetLOD;
            ME::MeshLoader::AddMeshToPending(rc->GetMesh()->GetMeshPath(), id);
        }
    }

    void StreamingManager::ProcessMipMap(unsigned long id, EntityState& state, float distance) {
        auto* rc = ME::ECS::GetComponent<RenderizableComponent>(id);
        if (!rc || !rc->GetTexture() || rc->IsColorActive()) return;

        if (state.max_mipmaps_available_ == 0) state.max_mipmaps_available_ = rc->GetTexture()->GetMipMapsQuantity();

        int targetMipMap = GetTargetNewIndex(distance, state.max_mipmaps_available_);

        if (targetMipMap != state.current_mipmap_) { // NECESITA CAMBIO DE MALLA
            if (rc->GetTexture()->IsMipMapPending()) { // YA SE EST CAMBIANDO
                state.current_mipmap_ = targetMipMap;
                return;
            }

            rc->GetTexture()->SetMipMap(targetMipMap);
            state.current_mipmap_ = targetMipMap;
            ME::TextureLoader::AddTextureToPending(rc->GetTexture()->GetTexturePath(), id);
        }
    }

    int StreamingManager::GetTargetNewIndex(float distance, int maxAvailable) {
        int lod = 0;
        for (size_t i = 0; i < lod_thresholds_.size(); ++i) {
            if (distance > lod_thresholds_[i]) lod = (int)i + 1;
            else break;
        }
        return std::min(lod, maxAvailable);
    }

    int StreamingManager::GetTargetQualityByMetric(float metric, int currentLevel, int maxAvailable, size_t currentAssetMem, size_t currentTotalBytes, size_t budgetBytes) {
        int desiredQuality = 5;
        if (metric >= 100.0f) desiredQuality = 0;
        else if (metric >= 50.0f) desiredQuality = 1;
        else if (metric >= 10.0f) desiredQuality = 2;
        else if (metric >= 1.0f)  desiredQuality = 3;
        else if (metric >= 0.1f)  desiredQuality = 4;
        else                      desiredQuality = 5;

        desiredQuality = std::min(desiredQuality, maxAvailable);

        if (currentLevel > desiredQuality) {
            size_t estimatedNextMem = currentAssetMem * 4;
            size_t memoryIncrease = estimatedNextMem - currentAssetMem;
            size_t maxAllowedSingleIncrease = budgetBytes * 0.25;

            if (memoryIncrease > maxAllowedSingleIncrease || (currentTotalBytes + memoryIncrease > budgetBytes)) {
                return currentLevel;
            }
        }

        return desiredQuality;
    }

    void StreamingManager::SetNodeVisibility(OctreeNode* node, bool visible) {
        for (unsigned long id : node->entity_ids_) {
            auto* rc = ME::ECS::GetComponent<RenderizableComponent>(id);
            if (rc && rc->Visible() != visible) {
                rc->SetVisibility(visible);
            }
        }

        // 2. Propagar a hijos
        if (!node->is_leaf_) {
            for (int i = 0; i < 8; ++i) {
                if (node->children_[i]) {
                    SetNodeVisibility(node->children_[i].get(), visible);
                }
            }
        }
    }

    void StreamingManager::Subdivide(OctreeNode* node) {
        
        node->is_leaf_ = false;
        Vec3 mid;
        mid.x_ = (node->min_bound_.x_ + node->max_bound_.x_) * 0.5f;
        mid.y_ = (node->min_bound_.y_ + node->max_bound_.y_) * 0.5f;
        mid.z_ = (node->min_bound_.z_ + node->max_bound_.z_) * 0.5f;

        for (int i = 0; i < 8; ++i) {
            node->children_[i] = std::make_unique<OctreeNode>();
            OctreeNode* child = node->children_[i].get();
            
            // X AXIS
            if (i & 1) {
                child->min_bound_.x_ = mid.x_;
                child->max_bound_.x_ = node->max_bound_.x_;
            }
            else {
                child->min_bound_.x_ = node->min_bound_.x_;
                child->max_bound_.x_ = mid.x_;
            }

            // Eje Y
            if (i & 2) {
                child->min_bound_.y_ = mid.y_;
                child->max_bound_.y_ = node->max_bound_.y_;
            }
            else {
                child->min_bound_.y_ = node->min_bound_.y_;
                child->max_bound_.y_ = mid.y_;
            }

            // Eje Z
            if (i & 4) {
                child->min_bound_.z_ = mid.z_;
                child->max_bound_.z_ = node->max_bound_.z_;
            }
            else {
                child->min_bound_.z_ = node->min_bound_.z_;
                child->max_bound_.z_ = mid.z_;
            }
            child->is_leaf_ = true;
        }

        for (unsigned long id : node->entity_ids_) {
            auto* tc = ME::ECS::GetComponent<TransformComponent>(id);
            if (!tc) continue;
            float pos[3] = { tc->Position().x_, tc->Position().y_, tc->Position().z_ };
            bool placed = false;
            for (int i = 0; i < 8; ++i) {
                if (node->children_[i].get()->Contains(pos[0], pos[1], pos[2])) {
                    node->children_[i]->entity_ids_.push_back(id);
                    current_entities_[id].current_node_ = node->children_[i].get();
                    placed = true;
                    break;
                }
            }
            if (!placed) {
                node->children_[0]->entity_ids_.push_back(id);
                current_entities_[id].current_node_ = node->children_[0].get();
            }
        }
        node->entity_ids_.clear();
    }
}
