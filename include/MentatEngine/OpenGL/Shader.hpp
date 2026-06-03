/**
 *
 * @brief Shader base class.
 * @author Ferran Barba, ESAT 2025-2026
 * @subject Engine Programming
 *
 */

#ifndef __SHADER_HPP__
#define __SHADER_HPP__ 1

namespace ME {
	class Shader {
		public:
            /**
            * @brief Default constructor.
            * Initializes all shader stage handles to 0 (invalid).
            */
            Shader() : vertexShader_{ 0 }, fragmentShader_{ 0 }, geometryShader_{ 0 } {};

            /**
             * @brief Constructs and compiles a basic shader program.
             * @param vertex Source code string for the vertex shader.
             * @param fragment Source code string for the fragment shader.
             */
            Shader(const char* vertex, const char* fragment);

            /**
             * @brief Constructs and compiles a shader program with geometry stage support.
             * @param vertex Source code string for the vertex shader.
             * @param fragment Source code string for the fragment shader.
             * @param geometry Source code string for the geometry shader.
             */
            Shader(const char* vertex, const char* fragment, const char* geometry);

            Shader(const Shader&) = delete;
            Shader& operator=(const Shader&) = delete;

            Shader(Shader&& other) noexcept;
            Shader& operator=(Shader&& other) noexcept;

            ~Shader();

            /** 
             * @brief Returns the OpenGL handle for the vertex shader stage. 
             */
            GLuint GetVertexShader() const;

            /** 
             * @brief Returns the OpenGL handle for the fragment shader stage. 
             */
            GLuint GetFragmentShader() const;

            /** 
             * @brief Returns the OpenGL handle for the geometry shader stage. 
             */
            GLuint GetGeometryShader() const;

            /**
             * @brief Deletes all compiled shaders from the OpenGL context.
             * Resets internal handles to 0.
             */
            void CleanShader();

		private:
			GLuint vertexShader_, fragmentShader_, geometryShader_;
	};
}

#endif // __SHADER_HPP__