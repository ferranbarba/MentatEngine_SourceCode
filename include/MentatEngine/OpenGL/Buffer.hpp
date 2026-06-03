/**
 *
 * @brief Buffer class.
 * @author Ferran Barba, ESAT 2025-2026
 * @subject Engine Programming
 *
 */

#ifndef __BUFFER_HPP__
#define __BUFFER_HPP__ 1

namespace ME {
	class Buffer {
		public:
            /**
             * @brief Default constructor that initializes an invalid buffer handle.
             */
            Buffer() : id_{ 0 }, target_{ 0 } {};

            /**
             * @brief Constructs a buffer associated with a specific OpenGL target.
             * @param target The GLenum target (e.g., GL_ARRAY_BUFFER, GL_ELEMENT_ARRAY_BUFFER).
             */
            Buffer(GLenum target);

            Buffer(const Buffer&) = delete;
            Buffer& operator=(const Buffer&) = delete;

            Buffer(Buffer&& other) noexcept : id_{ other.id_ }, target_{ other.target_ } {
                other.id_ = 0;
            }

            Buffer& operator=(Buffer&& other) noexcept;

            ~Buffer();

            /**
             * @brief Binds the buffer to its assigned OpenGL target.
             */
            void Bind() const;

            /**
             * @brief Unbinds the buffer from its assigned OpenGL target.
             */
            void Unbind() const;

            /**
             * @brief Unbinds a buffer from a specific OpenGL target.
             * @param target The GLenum target to unbind from.
             */
            void Unbind(GLenum target) const;

            /**
             * @brief Allocates and initializes the buffer's data store.
             * @param data Pointer to the source data.
             * @param size Size of the data in bytes.
             * @param usage Expected usage pattern (e.g., GL_STATIC_DRAW).
             */
            void SetData(const void* data, GLuint size, GLenum usage) const;

            /**
             * @brief Updates the existing data store of the buffer.
             * @param data Pointer to the new data.
             * @param size Size of the data in bytes.
             * @param usage The usage pattern to apply.
             */
            void UpdateData(const void* data, GLuint size, GLenum usage) const;

            /**
             * @brief Returns the raw OpenGL handle for the buffer.
             */
            GLuint ID() const { return id_; }


		private:
			GLuint id_;
			GLenum target_;
	};
}

#endif // __BUFFER_HPP__