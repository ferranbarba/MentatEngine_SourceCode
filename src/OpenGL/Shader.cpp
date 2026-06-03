#include <GL/glew.h>  
#include <fstream>
#include <sstream>
#include <iostream>  
#include <stdexcept> // Necesario para std::runtime_error
#include <../include/MentatEngine/OpenGL/Shader.hpp>

static std::string ReadTextFile(const char* path) {
	std::ifstream file(path, std::ios::in);
	if (!file.is_open()) {
		std::cerr << "Shader file not found: " << (path ? path : "(null)") << "\n";
		return {};
	}
	std::stringstream ss;
	ss << file.rdbuf();
	return ss.str();
}

static bool CompileAndLog(GLuint shader, const char* label) {
	glCompileShader(shader);
	GLint compiled = 0;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);

	if (!compiled) {
		GLint logLen = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLen);

		std::string log;
		log.resize((logLen > 1) ? logLen : 1);

		glGetShaderInfoLog(shader, logLen, nullptr, log.data());

		std::cerr << "ERROR::SHADER::COMPILATION_FAILED [" << (label ? label : "Unknown") << "]\n";
		std::cerr << log << "\n";
		return false;
	}
	return true;
}

namespace ME {
	Shader::Shader(const char* vertexPath, const char* fragmentPath) :
		vertexShader_{ 0 }, fragmentShader_{ 0 }, geometryShader_{ 0 }
	{
		try {
			// ---------- VERTEX ----------
			if (vertexPath) {
				std::string src = ReadTextFile(vertexPath);

				if (!src.empty()) {
					vertexShader_ = glCreateShader(GL_VERTEX_SHADER);

					const char* csrc = src.c_str();
					glShaderSource(vertexShader_, 1, &csrc, nullptr);

					if (!CompileAndLog(vertexShader_, vertexPath)) {
						throw std::runtime_error("Failed to compile Vertex Shader.");
					}
				}
				else {
					throw std::runtime_error("Vertex shader file is EMPTY or missing.");
				}
			}

			// ---------- FRAGMENT ----------
			if (fragmentPath) {
				std::string src = ReadTextFile(fragmentPath);

				if (!src.empty()) {
					fragmentShader_ = glCreateShader(GL_FRAGMENT_SHADER);

					const char* csrc = src.c_str();
					glShaderSource(fragmentShader_, 1, &csrc, nullptr);

					if (!CompileAndLog(fragmentShader_, fragmentPath)) {
						throw std::runtime_error("Failed to compile Fragment Shader.");
					}
				}
				else {
					throw std::runtime_error("Fragment shader file is EMPTY or missing.");
				}
			}
		}
		catch (...) {
			CleanShader();
			throw;
		}
	}

	Shader::Shader(const char* vertex, const char* fragment, const char* geometry) :
		vertexShader_{ 0 }, fragmentShader_{ 0 }, geometryShader_{ 0 }
	{
		try {
			// ---------- VERTEX ----------
			if (vertex) {
				std::string src = ReadTextFile(vertex);

				if (!src.empty()) {
					vertexShader_ = glCreateShader(GL_VERTEX_SHADER);

					const char* csrc = src.c_str();
					glShaderSource(vertexShader_, 1, &csrc, nullptr);

					if (!CompileAndLog(vertexShader_, vertex)) {
						throw std::runtime_error("Failed to compile Vertex Shader.");
					}
				}
				else {
					throw std::runtime_error("Vertex shader file is EMPTY or missing.");
				}
			}

			// ---------- FRAGMENT ----------
			if (fragment) {
				std::string src = ReadTextFile(fragment);

				if (!src.empty()) {
					fragmentShader_ = glCreateShader(GL_FRAGMENT_SHADER);

					const char* csrc = src.c_str();
					glShaderSource(fragmentShader_, 1, &csrc, nullptr);

					if (!CompileAndLog(fragmentShader_, fragment)) {
						throw std::runtime_error("Failed to compile Fragment Shader.");
					}
				}
				else {
					throw std::runtime_error("Fragment shader file is EMPTY or missing.");
				}
			}

			// ---------- GEOMETRY ----------
			if (geometry) {
				std::string src = ReadTextFile(geometry);

				if (!src.empty()) {
					geometryShader_ = glCreateShader(GL_GEOMETRY_SHADER);

					const char* csrc = src.c_str();
					glShaderSource(geometryShader_, 1, &csrc, nullptr);

					if (!CompileAndLog(geometryShader_, geometry)) {
						throw std::runtime_error("Failed to compile Geometry Shader.");
					}
				}
				else {
					// Corregido typo del log original ("Fragment shader EMPTY")
					throw std::runtime_error("Geometry shader file is EMPTY or missing.");
				}
			}
		}
		catch (...) {
			CleanShader();
			throw;
		}
	}

	ME::Shader::Shader(Shader&& other) noexcept
		: vertexShader_(other.vertexShader_),
		fragmentShader_(other.fragmentShader_),
		geometryShader_(other.geometryShader_) {
		other.vertexShader_ = 0;
		other.fragmentShader_ = 0;
		other.geometryShader_ = 0;
	}

	ME::Shader& ME::Shader::operator=(Shader&& other) noexcept {
		if (this != &other) {
			CleanShader();
			vertexShader_ = other.vertexShader_;
			fragmentShader_ = other.fragmentShader_;
			geometryShader_ = other.geometryShader_;
			other.vertexShader_ = 0;
			other.fragmentShader_ = 0;
			other.geometryShader_ = 0;
		}
		return *this;
	}

	Shader::~Shader() {
		CleanShader();
	}

	GLuint Shader::GetVertexShader() const {
		return vertexShader_;
	}

	GLuint Shader::GetFragmentShader() const {
		return fragmentShader_;
	}

	GLuint Shader::GetGeometryShader() const
	{
		return geometryShader_;
	}

	void Shader::CleanShader() {
		if (vertexShader_) {
			glDeleteShader(vertexShader_);
			vertexShader_ = 0;
		}
		if (fragmentShader_) {
			glDeleteShader(fragmentShader_);
			fragmentShader_ = 0;
		}
		if (geometryShader_) {
			glDeleteShader(geometryShader_);
			geometryShader_ = 0;
		}
	}
}
