#include <GL/glew.h>  
#include <../include/MentatEngine/OpenGL/VertexArray.hpp>

namespace ME {
	VertexArray::VertexArray() : id_{ 0 } {
		glGenVertexArrays(1, &id_);
	}

	VertexArray& VertexArray::operator=(VertexArray&& other) noexcept {
		if (this != &other) {
			if (id_ != 0) glDeleteVertexArrays(1, &id_);
			id_ = other.id_;
			other.id_ = 0;
		}
		return *this;
	}

	VertexArray::~VertexArray() {
		if (id_ != 0) glDeleteVertexArrays(1, &id_);
	}

	void VertexArray::Bind() const {
		glBindVertexArray(id_);
	}

	void VertexArray::Unbind() const {
		glBindVertexArray(0);
	}
}