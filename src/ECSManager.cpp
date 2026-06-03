/**
 *
 * @brief Entity Component System manager implementation.
 * @author Ferran Barba, ESAT 2025-2026
 * @subject Engine Programming
 *
 */

#ifndef __ANDROID__
#include <GLFW/glfw3.h>
#endif
#include <../include/MentatEngine/ECSManager.hpp>
#include <../include/MentatEngine/TransformComponent.hpp>
#include <../include/MentatEngine/Entity.hpp>
#include <../include/MentatEngine/Iterator.hpp>
#include <../include/MentatEngine/TagComponent.hpp>
#ifndef __ANDROID__
#include <../include/MentatEngine/StreamingManager.hpp>
#endif

namespace ME {

    // Definicin de la variable en el cpp al ser esttica
    std::unordered_map<std::size_t, std::unique_ptr<ECSListBase>> ECS::component_map_;
    unsigned long ECS::current_index;
    bool ECS::is_dirty;

    void ECS::SortLists()
    {
        for (auto& cl : component_map_) {
            cl.second->SortList();
        }
        is_dirty = false;
    }

    unsigned long ECS::AddEntity()
    {
        current_index++;
        return current_index - 1;
    }

    unsigned long ECS::AddEntityWithID(unsigned long forced_id)
    {
        if (forced_id >= current_index) {
            current_index = forced_id + 1;
        }
        return forced_id;
    }

    void ECS::Clear()
    {
        for (auto& cl : component_map_) {
            if (cl.second) {
                // Clear all registered component instances without unregistering the type.
                // We do it entity-wise only when needed elsewhere; here recreate empty lists by type is not possible
                // through ECSListBase without knowing T, so each concrete list implements ClearEntity only.
                // Instead, Android runtime registers fresh lists after process startup; for PC this remains unused.
            }
        }
        component_map_.clear();
        current_index = 0;
        is_dirty = false;
    }

    void ECS::RemoveEntity(unsigned long id) 
    {
        for (auto& cl : component_map_) {
            cl.second->ClearEntity(id);
        }
#ifndef __ANDROID__
        ME::StreamingManager::DeleteEntity(id);
#endif
    }

    std::vector<unsigned long> ECS::GetChildren(unsigned long id) {
        std::vector<unsigned long> child_ids;
        auto& transforms = ME::ECS::GetComponentList<ME::TagComponent>();

        for (auto& pair : transforms) {
            if (pair.second.GetParent() == id) child_ids.push_back(pair.first);
        }
       
        return child_ids;
    }
    unsigned long ECS::GetEntityByTag(const std::string& tag)
    {
        auto& tags = ME::ECS::GetComponentList<ME::TagComponent>();
        for (auto& pair : tags) {
            if (pair.second.GetTag() == tag) {
                return static_cast<unsigned long>(pair.first);
            }
        }
        return 0;
    }
}

