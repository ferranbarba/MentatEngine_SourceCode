#include <GL/glew.h>  
#include <../include/MentatEngine/OpenGL/Buffer.hpp>
#include <../include/MentatEngine/OpenGL/glMemoryTracker.hpp>

namespace ME {
	Buffer::Buffer(GLenum target) : id_{ 0 }, target_{ target } {
		glGenBuffers(1, &id_);
	}

	Buffer& Buffer::operator=(Buffer&& other) noexcept {
		if (this != &other) {
			if (id_ != 0) glDeleteBuffers(1, &id_);
			id_ = other.id_;
			target_ = other.target_;
			other.id_ = 0;
		}
		return *this;
	}

	Buffer::~Buffer() {
		if (id_ != 0) {
			//printf("DEBUG: Destruyendo Buffer ID %d\n", id_);
			ME::glMemoryTracker::UntrackBuffer(id_);
			glDeleteBuffers(1, &id_);
		}
	}

	void Buffer::Bind() const {
		glBindBuffer(target_, id_);
	}

	void Buffer::Unbind() const {
		glBindBuffer(target_, 0);
	}

	void Buffer::Unbind(GLenum target) const {
		glBindBuffer(target, 0);
	}

	void Buffer::SetData(const void* data, GLuint size, GLenum usage) const {
		// GL_ARRAY_BUFFER o GL_ELEMENT_ARRAY_BUFFER
		glBufferData(target_, size, data, usage); //GL_STATIC_DRAW

		// Buffer Tracker
		ME::glMemoryTracker::TrackBuffer(id_, size);
	}

	void Buffer::UpdateData(const void* data, GLuint size, GLenum usage) const {
		glBufferSubData(target_, 0, size, data); //GL_STATIC_DRAW
	}
}
