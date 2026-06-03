/**
 *
 * @brief Spatial streaming manager and octree data structures.
 * @author Sergi Sirvent, ESAT 2025-2026
 * @subject Engine Programming
 *
 */

#ifndef _STREAMING_MANAGER_H__
#define _STREAMING_MANAGER_H__ 1

#include <vector>
#include <memory>
#include "../deps/KFS/include/mat4.h"
#include "../Include/MentatEngine/MeshLoader.hpp"
#include "../Include/MentatEngine/TextureLoader.hpp"

namespace ME {

    struct OctreeNode {
        Vec3 min_bound_;
        Vec3 max_bound_;

        std::vector<unsigned long> entity_ids_;
        std::unique_ptr<OctreeNode> children_[8];
        bool is_leaf_ = true;

        const size_t kMaxEntities = 8;

        bool Contains(float x, float y, float z) const {
            return (x >= min_bound_.x_ && x <= max_bound_.x_ &&
                y >= min_bound_.y_ && y <= max_bound_.y_ &&
                z >= min_bound_.z_ && z <= max_bound_.z_);
        }

        bool ContainsBoundingBox(const Vec3 otherMin, const  Vec3 otherMax) const {
            return (otherMin.x_ >= min_bound_.x_ && otherMax.x_ <= max_bound_.x_ &&
                otherMin.y_ >= min_bound_.y_ && otherMax.y_ <= max_bound_.y_ &&
                otherMin.z_ >= min_bound_.z_ && otherMax.z_ <= max_bound_.z_);
        }
    };

    struct Frustum {
        // 6 -> Left, Right, Up, Down, Near, Far
        // 4 -> x, y, z and w (distance)
        float planes[6][4];

        enum Visibility { OUTSIDE, INTERSECT, INSIDE };

        void UpdateFrustum(Mat4& mat) {
            planes[0][0] = mat.getValue(0, 3) + mat.getValue(0, 0);
            planes[0][1] = mat.getValue(1, 3) + mat.getValue(1, 0);
            planes[0][2] = mat.getValue(2, 3) + mat.getValue(2, 0);
            planes[0][3] = mat.getValue(3, 3) + mat.getValue(3, 0);

            planes[1][0] = mat.getValue(0, 3) - mat.getValue(0, 0);
            planes[1][1] = mat.getValue(1, 3) - mat.getValue(1, 0);
            planes[1][2] = mat.getValue(2, 3) - mat.getValue(2, 0);
            planes[1][3] = mat.getValue(3, 3) - mat.getValue(3, 0);

            planes[2][0] = mat.getValue(0, 3) + mat.getValue(0, 1);
            planes[2][1] = mat.getValue(1, 3) + mat.getValue(1, 1);
            planes[2][2] = mat.getValue(2, 3) + mat.getValue(2, 1);
            planes[2][3] = mat.getValue(3, 3) + mat.getValue(3, 1);

            planes[3][0] = mat.getValue(0, 3) - mat.getValue(0, 1);
            planes[3][1] = mat.getValue(1, 3) - mat.getValue(1, 1);
            planes[3][2] = mat.getValue(2, 3) - mat.getValue(2, 1);
            planes[3][3] = mat.getValue(3, 3) - mat.getValue(3, 1);

            planes[4][0] = mat.getValue(0, 3) + mat.getValue(0, 2);
            planes[4][1] = mat.getValue(1, 3) + mat.getValue(1, 2);
            planes[4][2] = mat.getValue(2, 3) + mat.getValue(2, 2);
            planes[4][3] = mat.getValue(3, 3) + mat.getValue(3, 2);

            planes[5][0] = mat.getValue(0, 3) - mat.getValue(0, 2);
            planes[5][1] = mat.getValue(1, 3) - mat.getValue(1, 2);
            planes[5][2] = mat.getValue(2, 3) - mat.getValue(2, 2);
            planes[5][3] = mat.getValue(3, 3) - mat.getValue(3, 2);

            for (int i = 0; i < 6; i++) {
                float length = std::sqrt(planes[i][0] * planes[i][0] +
                    planes[i][1] * planes[i][1] +
                    planes[i][2] * planes[i][2]);
                if (length != 0.0f) {
                    float invLength = 1.0f / length;
                    planes[i][0] *= invLength;
                    planes[i][1] *= invLength;
                    planes[i][2] *= invLength;
                    planes[i][3] *= invLength;
                }
            }
        }

        Frustum::Visibility CheckBox(const Vec3& min, const Vec3& max) const {
            bool intersection = false;

            for (int i = 0; i < 6; ++i) {
                const float* p = planes[i];

                float px = (p[0] >= 0) ? max.x_ : min.x_;
                float py = (p[1] >= 0) ? max.y_ : min.y_;
                float pz = (p[2] >= 0) ? max.z_ : min.z_;

                if (p[0] * px + p[1] * py + p[2] * pz + p[3] < -0.01f) {
                    return OUTSIDE;
                }

                float nx = (p[0] >= 0) ? min.x_ : max.x_;
                float ny = (p[1] >= 0) ? min.y_ : max.y_;
                float nz = (p[2] >= 0) ? min.z_ : max.z_;

                if (p[0] * nx + p[1] * ny + p[2] * nz + p[3] < 0.0f) {
                    intersection = true;
                }
            }

            return intersection ? INTERSECT : INSIDE;
        }
    };

    struct EntityState {
        int current_lod_ = -1;
        int max_lods_available_ = 0;
        int current_mipmap_ = -1;
        int max_mipmaps_available_ = 0;
        float current_metric_value_ = 0.0f;
        OctreeNode* current_node_ = nullptr;
    };

    class StreamingManager {
    public:
        StreamingManager();
        ~StreamingManager();

        /**
         * @brief Starts the root node.
         * @details Initializes the core sub-systems and instantiates the root node of the Octree spatial structure.
         */
        static void Start();

        /**
         * @brief Sets the global quality of the entities.
         * @param quality The new global quality level (0 to 4) to be applied system-wide.
         */
        static void UpdateGlobalQuality(int quality);

        /**
         * @brief Registers an entity and its base mesh (this will trigger GenerateLods if necessary).
         * @details Sets up the tracking state for a new entity. If the mesh files do not have cached LOD data on disk,
         * it initiates the simplification pipeline.
         * @param entityID Unique identifier of the entity to register.
         */
        static void RegisterEntity(unsigned long entityID);

        /**
         * @brief Updates the entity mesh when it is loaded.
         * @details Synchronizes component geometry metrics and state handles once the asynchronous loading thread finishes disk processing.
         * @param entityID Unique identifier of the entity.
         */
        static void UpdateEntityMesh(unsigned long entityID);

        /**
         * @brief Updates the entity texture when its loaded.
         * @details Binds and updates texture asset handles upon successful completion of the background streaming job.
         * @param entityID Unique identifier of the entity.
         */
        static void UpdateEntityTexture(unsigned long entityID);

        /**
         * @brief Deletes entity from the list of loaded entities.
         * @details Safely detaches the entity from active tracking loops and frees its corresponding runtime state data from memory.
         * @param entityID Unique identifier of the entity to remove.
         */
        static void DeleteEntity(unsigned long entityID);

        /**
         * @brief Reinserts on nodes, fills with new distances and applies changes.
         * @details Per-frame update entry point that updates camera-relative distances, re-balances spatial parenting inside the Octree,
         * and schedules resource paging orders.
         */
        static void Update();

        /**
         * @brief Returns the root node from the octree.
         * @return OctreeNode* Pointer to the top-level root node of the spatial bounding hierarchy.
         */
        static OctreeNode* GetRootNode() { return &root_node_; };

        /**
         * @brief Set the current mode for the memory budget (0: Auto, 1: Distance, 2: None).
         * @param mode The memory allocation budget enforcement strategy mode to activate.
         */
        static void SetBudgetMode(unsigned int mode) { budget_mode_ = mode; }

        /**
         * @brief Get the current mode for memory budget (0: Auto, 1: Distance, 2: None).
         * @return unsigned int The active execution mode for memory regulation.
         */
        static unsigned int GetBudgetMode() { return budget_mode_; }

        /**
         * @brief Returns the list of entities and their states.
         * @return std::unordered_map<unsigned long, EntityState>& Reference to the lookup map containing all tracked entities in the streaming pipeline.
         */
        static std::unordered_map<unsigned long, EntityState>& GetCurrentStreaming() { return current_entities_; }

        /**
         * @brief Returns the current lod thresholds.
         * @return std::vector<float>& Vector containing the distance thresholds used to clip quality layers.
         */
        static std::vector<float>& GetCurrentThresholds() { return lod_thresholds_; }

        /**
         * @brief Returns a map with a meshpath and the minimum distance of all the entities with that mesh.
         * @return std::unordered_map<std::string, float>& An associative map lookup indexed by the filepath string of the geometry asset.
         */
        static std::unordered_map<std::string, float>& GetCurrentMeshMinimumDistance() { return min_mesh_distances_; }

    private:

        /**
         * @brief Called to check if a change of parenting is needed.
         * @details Evaluation step to verify if an entity has drifted outside its current Octree cell bounds and must migrate to a different node.
         * @param entityID Unique identifier of the entity.
         */
        static void ReinsertEntity(unsigned long entityID);

        /**
         * @brief Updates mesh distances and metrics (volume / distance).
         * @details Recursively parses nodes to evaluate the visual weight and importance of active meshes based on screen-space coverage rules.
         * @param node The current OctreeNode being evaluated.
         * @param camX X-coordinate position of the rendering camera viewpoint.
         * @param camY Y-coordinate position of the rendering camera viewpoint.
         * @param camZ Z-coordinate position of the rendering camera viewpoint.
         */
        static void AnalyzeNode(OctreeNode* node, float camX, float camY, float camZ);

        /**
         * @brief Runs the octree to hide all not needed entities from the scene.
         * @details Executes a hierarchical frustum intersection test across the tree structure to cull out-of-bounds entities from the render queue.
         * @param node Root node of the specific sub-tree under inspection.
         */
        static void FrustumCulling(OctreeNode* node);

        /**
         * @brief Uses the current mode for the memory budget (0: Auto, 1: Distance, 2: None).
         * @details Main balancing loop that performs memory budget arbitration, trading detail layers against hard VRAM/buffer limits.
         * @param node Origin node containing the structural collection of targeted entities.
         * @param camX X-coordinate position of the rendering camera viewpoint.
         * @param camY Y-coordinate position of the rendering camera viewpoint.
         * @param camZ Z-coordinate position of the rendering camera viewpoint.
         */
        static void UpdateBaseOnBudget(OctreeNode* node, float camX, float camY, float camZ);

        /**
         * @brief Recursive entity insertion in the octree node.
         * @param node Bounding node targeting insertion layout criteria.
         * @param entityID Unique identifier of the entity to place.
         * @param x World space X coordinate position.
         * @param y World space Y coordinate position.
         * @param z World space Z coordinate position.
         * @param depth Current traversal depth layer in the tree hierarchy branching loop.
         */
        static void RecursiveInsertEntity(OctreeNode* node, unsigned long entityID, float x, float y, float z, int depth = 0);

        /**
         * @brief Recursive search for entity removal.
         * @details Deep searches child branches to disconnect, scrub, and un-parent an entity reference cleanly.
         * @param node Traversal start context handle node.
         * @param entityID Unique identifier of the target entity to wipe.
         */
        static void RecursiveRemoveEntity(OctreeNode* node, unsigned long entityID);

        /**
         * @brief Decides whether to change the mesh index based on distance.
         * @details Evaluates distance thresholds to determine if a new geometric LOD swap is required.
         * @param id Unique identifier of the entity.
         * @param state Reference wrapper tracking runtime entity indicators.
         * @param distance Computed linear distance to the viewport camera.
         */
        static void ProcessLOD(unsigned long id, EntityState& state, float distance);

        /**
         * @brief Checks if a change to texture Mipmaps is needed based on distance.
         * @details Evaluates step distances to schedule texture resolution down-scaling or up-scaling actions.
         * @param id Unique identifier of the entity.
         * @param state Reference wrapper tracking runtime entity indicators.
         * @param distance Computed linear distance to the viewport camera.
         */
        static void ProcessMipMap(unsigned long id, EntityState& state, float distance);

        /**
         * @brief Get the approximate value based on the metric.
         * @details Performs a memory-aware balance step to figure out the best asset quality index (LOD/Mipmap) by vetting
         * abstract visual metrics against discrete memory tracking metrics and free system headroom.
         * @param metric Abstract screen-space density area value.
         * @param currentLevel Current active quality index layer index inside tracking structures.
         * @param maxAvailable Maximum physical asset variation boundaries supported by the asset resource.
         * @param currentAssetMem VRAM buffer byte footprint weight of the current active layer.
         * @param currentTotalBytes Collective allocation byte size tally registered during the current context sweep.
         * @param budgetBytes Hardware memory safety cap boundary configuration target.
         * @return int Safe balanced target quality index layout (0 indicates peak fidelity asset execution).
         */
        static int GetTargetQualityByMetric(float metric, int currentLevel, int maxAvailable,
            size_t currentAssetMem, size_t currentTotalBytes, size_t budgetBytes);

        /**
         * @brief Sets all entities in the childs from a node to visible/not visible.
         * @param node Parent tree node containing sub-tree cluster elements.
         * @param visible True to include tracked render objects inside active passes, False to drop them.
         */
        static void SetNodeVisibility(OctreeNode* node, bool visible);

        /**
        * @brief Calculates the index of LOD/MipMap based on distance.
        * @param distance Camera-relative linear separation.
        * @param maxAvailable Maximum physical asset variation index configuration supported by the resource metadata.
        * @return int Targeted quality layout calculation index layer destination.
        */
        static int GetTargetNewIndex(float distance, int maxAvailable);

        /**
        * @brief Octree nodes subdivision.
        * @details Splits a structural volume block horizontally and vertically into 8 uniform sub-volumes.
        * @param node The parent Octree node designated for structural dividing.
        */
        static void Subdivide(OctreeNode* node);

        static unsigned int budget_mode_;
        static OctreeNode root_node_;
        static std::vector<float> lod_thresholds_;
        static std::unordered_map<std::string, float> min_mesh_distances_;
        static std::unordered_map<unsigned long, EntityState> current_entities_;
    };
}

#endif // _STREAMING_MANAGER_H__