#include <GL/glew.h>  
#include <iostream>  
#include <../include/MentatEngine/OpenGL/Program.hpp>

namespace ME {
	Program& Program::operator=(Program&& other) noexcept {
		if (this != &other) {
			if (program_) glDeleteProgram(program_);
			program_ = other.program_;
			linked_ = other.linked_;
			other.program_ = 0;
			other.linked_ = false;
		}
		return *this;
	}

	Program::~Program() {
		if (program_) {
			glDeleteProgram(program_);
		}
	}

	void Program::CreateProgram()
	{
		program_ = glCreateProgram();
		std::cerr << "[CreateProgram] program_id=" << program_ << "\n";
	}

	void Program::AttachShader(Shader* s) {
		if (program_ && s) {
			std::cerr << "[AttachShader] program_id=" << program_
				<< " v=" << (s ? s->GetVertexShader() : 0)
				<< " f=" << (s ? s->GetFragmentShader() : 0)
				<< " g=" << (s ? s->GetGeometryShader() : 0)
				<< "\n";
			if (s->GetVertexShader() != 0) {

				glAttachShader(program_, s->GetVertexShader());
				std::cerr << "[AttachShader] attached vertex\n";
			}
			if (s->GetFragmentShader() != 0) {
				glAttachShader(program_, s->GetFragmentShader());
				std::cerr << "[AttachShader] fragment vertex\n";
			}
			if (s->GetGeometryShader() != 0) {

				glAttachShader(program_, s->GetGeometryShader());
				std::cerr << "[AttachShader] geometry vertex\n";
			}
		}
	}

	void Program::LinkProgram() {
		if (program_) {
			glLinkProgram(program_);

			GLint linked = 0;
			glGetProgramiv(program_, GL_LINK_STATUS, &linked);

			if (!linked) {
				std::cerr << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n";

				GLint numAttached = 0;
				glGetProgramiv(program_, GL_ATTACHED_SHADERS, &numAttached);
				std::cerr << "Attached shaders: " << numAttached << "\n";

				GLint validated = 0;
				glValidateProgram(program_);
				glGetProgramiv(program_, GL_VALIDATE_STATUS, &validated);
				std::cerr << "Validate status: " << validated << "\n";

				GLint logLen = 0;
				glGetProgramiv(program_, GL_INFO_LOG_LENGTH, &logLen);
				std::cerr << "Info log length: " << logLen << "\n";

				if (logLen > 1) {
					std::string log;
					log.resize(logLen);
					GLsizei written = 0;
					glGetProgramInfoLog(program_, logLen, &written, log.data());
					std::cerr << "Info log written: " << written << "\n";
					std::cerr << log << "\n";
				}
				else {
					std::cerr << "(empty info log)\n";
				}
			}
		}
	}

	GLuint Program::GetProgram() {
		return program_;
	}
}