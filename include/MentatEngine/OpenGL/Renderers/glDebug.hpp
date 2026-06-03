/**
 *
 * @brief Program class.
 * @author Ferran Barba, ESAT 2025-2026
 * @subject Engine Programming
 *
  * glPhong.hpp
  * Renderer Phong multipass (una pasada por luz, acumulación aditiva).
  *
  * Idea clave:
  * - Usa el ECS y recorre componentes por "owner()" (id entidad).
  * - Para cada luz hace un pase de render de TODA la escena.
  * - Primer pase: escribe Z, sin blending.
  * - Pases siguientes: depthFunc = EQUAL y blending aditivo para sumar contribuciones.
  *
  * Consecuencia:
  * - Si tienes N luces y M entidades, haces N * M drawcalls (caro).
  */

#ifndef __GL_DEBUG__HPP__
#define __GL_DEBUG__HPP__ 1

#include <../../include/MentatEngine/OpenGL/Shader.hpp>
#include <../../include/MentatEngine/OpenGL/Program.hpp>
#include <../../include/MentatEngine/Iterator.hpp>
#include <../../deps/KFS/include/vec3.h>
#include <../../deps/KFS/include/mat4.h>

namespace ME {
    class glDebug {
    public:
        /**
         * @brief Constructor: compiles shaders, links the program, and caches uniforms.
         */
        glDebug();

        glDebug(glDebug&& other) noexcept : shader_(std::move(other.shader_)),
            program_(std::move(other.program_)){
        }

        glDebug& operator=(glDebug&& other) noexcept {
            if (this != &other) {
                shader_ = std::move(other.shader_);
                program_ = std::move(other.program_);
                debug_vbo_ = other.debug_vbo_;
                debug_vao_ = other.debug_vao_;
            }
            return *this;
        }

        glDebug(const glDebug&) = delete;
        glDebug& operator=(const glDebug&) = delete;


        void Render(GLuint fbo);

    private:

        void DrawOctreeDebug(OctreeNode* node, Mat4& view, Mat4& proj, int depth);
        void DrawBoundingBox(unsigned long entityID, Vec3 color, Mat4& view, Mat4& proj);
        void DrawWireCube(const Vec3 min, const Vec3 max, float r, float g, float b, Mat4& view, Mat4& proj, int depth);
        /**
         * @brief Helper that initializes ECS iterators for a specific component list.
         *
         * Sets the start and end iterators. If the container is empty,
         * it marks them as empty to prevent invalid access during Render().
         *
         * @tparam T The component type to iterate over.
         * @param it Pointer to the start iterator to be initialized.
         * @param end Pointer to the end iterator to be initialized.
         * @param first Pointer to the first element's data if available.
         */
        template<typename T>
        void SetIterators(ME::Iterator<T>* it, ME::Iterator<T>* end, T** first) {
            ME::ContainerFacade<T> container{ ME::ECS::GetComponentList<T>() };

            if (!container.isEmpty()) {
                *it = container.begin();
                it->SetEmpty(false);
                *first = **it;
                *end = container.end();
                end->SetEmpty(false);
            }
            else {
                it->SetEmpty(true);
                end->SetEmpty(true);
            }
        }

        /**
         * @brief Helper that advances the component iterator if it matches the current entity.
         *
         * Assumes that the ECS component lists are sorted by entity ID (owner).
         *
         * @tparam T The component type.
         * @param it Pointer to the current iterator.
         * @param end Pointer to the end iterator.
         * @param first Pointer to the current component data.
         * @param current The ID of the entity currently being processed.
         */
        template<typename T>
        void CheckIteratorsFinished(ME::Iterator<T>* it, ME::Iterator<T>* end, T** first, int current) {
            if (*it != *end && it->owner() == current && !it->IsEmpty()) {
                ++(*it);
                if (*it != *end) {
                    *first = **it;
                }
            }
        }

        // Debug Info
        Program program_;
        Shader shader_;

        struct DebugUniforms {
            GLint uView = -1;
            GLint uProj = -1;
            GLint uColor = -1;
        };

        DebugUniforms debug_u_;
        GLuint debug_vbo_ = 0;
        GLuint debug_vao_ = 0;
    };
}

#endif // __GL_DEBUG__HPP__