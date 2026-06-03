/**
 *
 * @brief VertexArray class.
 * @author Ferran Barba, ESAT 2025-2026
 * @subject Engine Programming
 *
 */

#ifndef __VERTEX_ARRAY_HPP__
#define __VERTEX_ARRAY_HPP__ 1

namespace ME {
	class VertexArray {
		public:
            /**
             * @brief Default constructor.
             * Generates a new Vertex Array Object (VAO) name on the GPU.
             */
            VertexArray();

            VertexArray(const VertexArray&) = delete;
            VertexArray& operator=(const VertexArray&) = delete;

            VertexArray(VertexArray&& other) noexcept : id_{ other.id_ } {
                other.id_ = 0; // Invalidates the source ID
            }

            VertexArray& operator=(VertexArray&& other) noexcept;

            ~VertexArray();

            /**
             * @brief Binds the Vertex Array Object to the current OpenGL context.
             */
            void Bind() const;

            /**
             * @brief Unbinds any Vertex Array Object from the current OpenGL context.
             */
            void Unbind() const;

            /**
             * @brief Gets the underlying OpenGL handle for the VAO.
             * @return The GLuint ID assigned by the driver.
             */
            GLuint GetVAO() const { return id_; };

		private:
			GLuint id_;
	};
}

#endif // __VERTEX_ARRAY_HPP__