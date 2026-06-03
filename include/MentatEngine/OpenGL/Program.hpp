/**
 *
 * @brief Program class.
 * @author Ferran Barba, ESAT 2025-2026
 * @subject Engine Programming
 *
 */

#ifndef __PROGRAM_HPP__
#define __PROGRAM_HPP__ 1

#include <../include/MentatEngine/OpenGL/Shader.hpp>

namespace ME {
	class Program {
		public:
            /**
             * @brief Default constructor that initializes the program as invalid and unlinked.
             */
            Program() : program_{ 0 }, linked_{ false } {};

            Program(const Program&) = delete;
            Program& operator=(const Program&) = delete;

            Program(Program&& other) noexcept : program_(other.program_), linked_(other.linked_)
            {
                other.program_ = 0;
                other.linked_ = false;
            }

            Program& operator=(Program&& other) noexcept;

            ~Program();

            /**
             * @brief Generates a new OpenGL program object name.
             */
            void CreateProgram();

            /**
             * @brief Attaches a compiled shader stage to this program.
             * @param s Pointer to the compiled Shader object containing the stages.
             */
            void AttachShader(Shader* s);

            /**
             * @brief Links all attached shaders into a final executable GPU program.
             * Checks for linking errors internally.
             */
            void LinkProgram();

            /**
             * @brief Returns the raw OpenGL handle for the linked program.
             */
            GLuint GetProgram();

            /**
             * @brief Checks if the program has been successfully linked and is ready for use.
             */
            bool IsLinked() const { return linked_; }

		private:
			GLuint program_;
			bool linked_;
	};
}

#endif // __PROGRAM_HPP__