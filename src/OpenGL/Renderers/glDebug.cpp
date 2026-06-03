#include <GL/glew.h>  
#include <iostream>  
#include "../Include/MentatEngine/ImGuiManager.hpp"
#include <../include/MentatEngine/OpenGL/OpenGLAPI.hpp>
#include <../include/MentatEngine/OpenGL/RenderizableComponent.hpp>
#include <../include/MentatEngine/TransformComponent.hpp>
#include <../include/MentatEngine/LightComponent.hpp>
#include <../include/MentatEngine/ScriptComponent.hpp>
#include <../Include/MentatEngine/CameraComponent.hpp>
#include <../../include/MentatEngine/OpenGL/Renderers/glDebug.hpp>
#include <../../include/MentatEngine/OpenGL/glMemoryTracker.hpp>

namespace ME {
    // Helper local: pide la location de un uniform en el programa.
    // Devuelve -1 si no existe/no está activo (por ejemplo optimizado por el compilador).
    static GLint getUniformValue(GLuint prog, const char* name) {
        GLint loc = glGetUniformLocation(prog, name);
        return loc;
    }

    glDebug::glDebug()
		: shader_(Shader("../../deps/shaders/OctreeDebug/vertex.vert", "../../deps/shaders/OctreeDebug/fragment.frag")), program_(Program()) {
		
        program_.CreateProgram();
        program_.AttachShader(&shader_);
        program_.LinkProgram();
		shader_.CleanShader();

        debug_u_.uView = glGetUniformLocation(program_.GetProgram(), "uView");
        debug_u_.uProj = glGetUniformLocation(program_.GetProgram(), "uProj");
        debug_u_.uColor = glGetUniformLocation(program_.GetProgram(), "uColor");

        // VAO para una sola línea (2 puntos)
        glGenVertexArrays(1, &debug_vao_);
        glGenBuffers(1, &debug_vbo_);
        glBindVertexArray(debug_vao_);
        glBindBuffer(GL_ARRAY_BUFFER, debug_vbo_);
        // Reservamos espacio para 2 vértices (x,y,z)
        glBufferData(GL_ARRAY_BUFFER, 6 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
	}

    void glDebug::Render(GLuint fbo) {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glViewport(0, 0, 1280, 720);

        Mat4 view, proj;
        view.identity();
        proj.identity();

        {
            Iterator<CameraComponent> cIt, cEnd;
            CameraComponent* cam = nullptr;
            SetIterators(&cIt, &cEnd, &cam);

            if (cIt != cEnd && cam && cam->active)
            {
                view = cam->view;
                proj = cam->proj;
            }
        }

        glUseProgram(program_.GetProgram());
        glBindVertexArray(debug_vao_);
        glDisable(GL_DEPTH_TEST);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

        DrawOctreeDebug(ME::StreamingManager::GetRootNode(), view, proj, 0);

        ME::Iterator<ME::RenderizableComponent> r_it, r_end;
        ME::RenderizableComponent* r = nullptr;
        SetIterators(&r_it, &r_end, &r);

        unsigned int e = 0;
        while (r_it != r_end) {
            if (r && r_it.owner() == e) {
                DrawBoundingBox(e, Vec3(1, 0, 0), view, proj); // Dibujar en rojo
            }
            CheckIteratorsFinished(&r_it, &r_end, &r, (int)e);
            e++;
        }

        // Limpieza y vuelta al Framebuffer 0 para ImGui
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glEnable(GL_DEPTH_TEST);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void glDebug::DrawOctreeDebug(OctreeNode* node, Mat4& view, Mat4& proj, int depth) {
        if (!node) return;

        Vec3 color;
        bool has_entities = !node->entity_ids_.empty();

        if (node->is_leaf_) {
            if (has_entities) {
                color = Vec3(0.0f, 1.0f, 0.0f);
            } else {
                color = Vec3(0.15f, 0.15f, 0.15f);
            }
        } else {
            color = Vec3(0.15f, 0.15f, 0.15f);
        }

        if (depth > 0) {
            DrawWireCube(node->min_bound_, node->max_bound_, color.x_, color.y_, color.z_, view, proj, depth);
        }

        if (!node->is_leaf_) {
            for (int i = 0; i < 8; ++i) {
                if (node->children_[i]) {
                    DrawOctreeDebug(node->children_[i].get(), view, proj, depth + 1);
                }
            }
        }
    }

    void glDebug::DrawBoundingBox(unsigned long entityID, Vec3 color, Mat4& view, Mat4& proj) {
        auto* tc = ME::ECS::GetComponent<TransformComponent>(entityID);
        auto* rc = ME::ECS::GetComponent<RenderizableComponent>(entityID);

        if (!rc || !rc->GetMesh() || !tc) return;

        std::pair<Vec3, Vec3> bb = rc->GetBoundingBox(tc->WorldMatrix());

        DrawWireCube(bb.first, bb.second, color.x_, color.y_, color.z_, view, proj, 0);
    }

    void glDebug::DrawWireCube(const Vec3 min, const Vec3 max, float r, float g, float b, Mat4& view, Mat4& proj, int depth) {
        float offset = depth * 0.005f;

        float v[8][3] = {
            { min.x_ + offset, min.y_ + offset, min.z_ + offset }, // 0
            { max.x_ - offset, min.y_ + offset, min.z_ + offset }, // 1
            { max.x_ - offset, max.y_ - offset, min.z_ + offset }, // 2
            { min.x_ + offset, max.y_ - offset, min.z_ + offset }, // 3
            { min.x_ + offset, min.y_ + offset, max.z_ - offset }, // 4
            { max.x_ - offset, min.y_ + offset, max.z_ - offset }, // 5
            { max.x_ - offset, max.y_ - offset, max.z_ - offset }, // 6
            { min.x_ + offset, max.y_ - offset, max.z_ - offset }  // 7
        };

        glUseProgram(program_.GetProgram());
        glBindVertexArray(debug_vao_);
        glUniformMatrix4fv(debug_u_.uView, 1, GL_FALSE, view.M_);
        glUniformMatrix4fv(debug_u_.uProj, 1, GL_FALSE, proj.M_);
        glUniform3f(debug_u_.uColor, r, g, b);

        // Definimos los índices de las 12 líneas (24 vértices) para dibujarlas de golpe
        int indices[24] = { 0,1, 1,2, 2,3, 3,0, 4,5, 5,6, 6,7, 7,4, 0,4, 1,5, 2,6, 3,7 };

        for (int i = 0; i < 24; i += 2) {
            float line[6] = { v[indices[i]][0], v[indices[i]][1], v[indices[i]][2],
                              v[indices[i + 1]][0], v[indices[i + 1]][1], v[indices[i + 1]][2] };

            glBindBuffer(GL_ARRAY_BUFFER, debug_vbo_);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(line), line);
            glDrawArrays(GL_LINES, 0, 2);
        }
    }
}