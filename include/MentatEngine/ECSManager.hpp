/**
 *
 * @brief Entity Component System manager and component storage.
 * @author Ferran Barba, ESAT 2025-2026
 * @subject Engine Programming
 *
 */

#ifndef __ECS_MANAGER_HPP__
#define __ECS_MANAGER_HPP__ 1

#include <vector>
#include <memory>
#include <unordered_map>
#include <optional>
#include <algorithm>
#include <stdio.h>
#include <string>


namespace ME {

    class ECSListBase{
        public:
            virtual size_t Size() = 0;
            virtual void SortList() = 0;
            virtual void ClearEntity(unsigned long id) = 0;
    };

    template<typename T>
    class ECSList : public ECSListBase {
    public:
        /** 
         * @brief Initializes the list with a reserved capacity of 50 elements.
         */
        ECSList() {
            list.reserve(50);
        }

        /** 
         * @brief Returns the number of components in the list.
         */
        virtual size_t Size() { return list.size(); }

        /**
         * @brief Sorts components by entity ID (asc) to maintain cache-friendly access.
         */
        virtual void SortList() { 
            std::sort(list.begin(), list.end(),
                [](const std::pair<size_t, T>& a,
                    const std::pair<size_t, T>& b) {
                        return a.first < b.first;
                }
            );
        }

        /** 
         * @brief Removes a component associated with a specific entity ID.
         */
        virtual void ClearEntity(unsigned long id) { 
            int it = 0;
            for (std::pair<size_t, T>& p : list) {
                if (p.first == id) {
                    list.erase(list.begin() + it);
                    return;
                }
                it++;
            }
        };

        std::vector<std::pair<size_t, T>> list;
    };

    class ECS {
        public:
            /**
             * @brief Retrieves the raw list of components for a specific type.
             * @tparam T The component type.
             * @return A reference to the vector of entity-component pairs.
             */
            template<typename T>
            static std::vector<std::pair<size_t, T>>& GetComponentList()
            {
                const std::size_t hash = typeid(T).hash_code();
                auto it = component_map_.find(hash);

                // Never dereference an invalid iterator here. Some systems query a
                // component list before the type has been explicitly registered;
                // create the list lazily so callers receive a valid, empty storage
                // instead of triggering undefined behaviour.
                if (it == component_map_.end()) {
                    auto inserted = component_map_.emplace(hash, std::make_unique<ECSList<T>>());
                    it = inserted.first;
                }

                return static_cast<ECSList<T>*>(it->second.get())->list;
            }

            /**
             * @brief Searches for a component of type T on a specific entity.
             * @param entity The ID of the entity.
             * @return Pointer to the component if it exists, nullptr otherwise.
             */
            template<typename T>
            static T* GetComponent(unsigned long entity)
            {
                if (is_dirty) {
                    SortLists();
                }

                std::vector<std::pair<size_t, T>>& temp_list = ECS::GetComponentList<T>();

                // Binary search O(log n)
                auto it = std::lower_bound(temp_list.begin(), temp_list.end(), entity,
                    [](const std::pair<size_t, T>& element, unsigned long id) {
                        return element.first < id;
                    }
                );

                if (it != temp_list.end() && it->first == entity) {
                    return &(it->second);
                }

                return nullptr;
            }

            /**
             * @brief Adds or retrieves a component for an entity.
             *
             * Constructs the component in-place if it doesn't exist. Sets #is_dirty to true
             * if the addition breaks the sorted order of the list.
             *
             * @tparam T Component type.
             * @tparam Args Argument types for component construction.
             * @param entity The ID of the target entity.
             * @param args Arguments forwarded to the component's constructor.
             * @return Reference to the added or existing component.
             */
            template<typename T, typename... Args>
            static T& AddComponent(unsigned long entity, Args&&... args)
            {
                std::vector<std::pair<size_t, T>>& temp_list = ECS::GetComponentList<T>();

                for (auto& p : temp_list) {
                    if (p.first == entity) {
                        return p.second;
                    }
                }

                if (temp_list.size() > 0) {
                    if (entity < temp_list[temp_list.size() - 1].first) {
                        is_dirty = true;
                    }
                }

                // Construction in-place of the pair
                temp_list.emplace_back(
                    std::piecewise_construct,
                    std::forward_as_tuple(entity),
                    std::forward_as_tuple(std::forward<Args>(args)...)
                );

                return temp_list.back().second;
            }

            /**
            * @brief Registers a new component type into the ECS internal map.
            * @tparam T The component type to register.
            */
            template<typename T>
            static void AddComponentList()
            {
                const std::size_t hash = typeid(T).hash_code();
                if (component_map_.find(hash) == component_map_.end()) {
                    component_map_.emplace(hash, std::make_unique<ECSList<T>>());
                }
            }

            /**
            * @brief Removes a specific component type from an entity.
            * @tparam T The component type to remove.
            * @param entity The ID of the entity.
            */
            template<typename T>
            static void RemoveComponent(unsigned long entity)
            {
                std::size_t hash = typeid(T).hash_code();
                for (auto& cl : component_map_) {
                    if (cl.first == hash) {
                        cl.second->ClearEntity(entity);
                    }
                }
                is_dirty = true;
            }

            /** 
             * @brief Sorts all component lists that have been marked as dirty.
             */
            static void SortLists();

            /** 
             * @brief Creates a new unique entity ID. */
            static unsigned long AddEntity();

            /**
             * @brief Registers a specific entity ID. Useful when instantiating serialized scenes.
             */
            static unsigned long AddEntityWithID(unsigned long forced_id);

            /**
             * @brief Clears all registered component lists and resets the entity counter.
             */
            static void Clear();

            /** 
             * @brief Destroys an entity and all its associated components.
             */
            static void RemoveEntity(unsigned long id);

            /** 
             * @brief Returns a list of children IDs for a given entity.
             */
            static std::vector<unsigned long> GetChildren(unsigned long id);

            /**
             * @brief Returns the real entity ID that owns the requested tag.
             * @param tag Tag value to search.
             * @return Entity ID if found, or 0 when no entity has that tag.
             */
            static unsigned long GetEntityByTag(const std::string& tag);

            static unsigned long current_index;

            static bool is_dirty;

        private:

            static std::unordered_map<std::size_t, std::unique_ptr<ECSListBase>> component_map_;
    };
}

#endif //__ECS_MANAGER_HPP__
