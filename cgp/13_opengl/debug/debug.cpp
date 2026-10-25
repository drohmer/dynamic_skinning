#include "debug.hpp"

#include "cgp/01_base/base.hpp"
#include <iostream>

namespace cgp
{
	static std::string gl_string_safe(GLenum name)
	{
		// glGetString may return nullptr (no active context, or GL error).
		//  Constructing a std::string from a null const char* is undefined behaviour.
		GLubyte const* value = glGetString(name);
		if (value == nullptr)
			return "(unavailable)";
		return std::string(reinterpret_cast<char const*>(value));
	}

	std::string opengl_info_display()
	{
		std::string s;
		s += "[VENDOR]      : " + gl_string_safe(GL_VENDOR) + "\n";
		s += "[RENDERER]    : " + gl_string_safe(GL_RENDERER) + "\n";
		s += "[VERSION]     : " + gl_string_safe(GL_VERSION) + "\n";
		s += "[GLSL VERSION]: " + gl_string_safe(GL_SHADING_LANGUAGE_VERSION) + "\n";

        return s;
	}

	static std::string opengl_error_to_string(GLenum error)
    {
        switch(error)
        {
        case GL_NO_ERROR:
            return "GL_NO_ERROR";
        case GL_INVALID_ENUM:
            return "GL_INVALID_ENUM";
        case GL_INVALID_VALUE:
            return "GL_INVALID_VALUE";
        case GL_INVALID_OPERATION:
            return "GL_INVALID_OPERATION";
        case GL_INVALID_FRAMEBUFFER_OPERATION:
            return "GL_INVALID_FRAMEBUFFER_OPERATION";
        case GL_OUT_OF_MEMORY:
            return "GL_OUT_OF_MEMORY";
    #ifndef __EMSCRIPTEN__
        case GL_STACK_UNDERFLOW:
            return "GL_STACK_UNDERFLOW";
        case GL_STACK_OVERFLOW:
            return "GL_STACK_OVERFLOW";
    #endif
        default:
            return "UNKNOWN";
        }
    }
	void check_opengl_error(std::string const& file, std::string const& function, int line)
	{
        // The OpenGL error queue can hold several errors: drain it entirely, otherwise
        //  a stale error would be wrongly attributed to the next opengl_check call.
        std::string errors;
        for (GLenum error = glGetError(); error != GL_NO_ERROR; error = glGetError())
        {
            if (errors.empty() == false)
                errors += ", ";
            errors += opengl_error_to_string(error);
        }

        if (errors.empty() == false)
        {
            std::string msg = "OpenGL ERROR detected\n"
                    "\tFile "+file+"\n"
                    "\tFunction "+function+"\n"
                    "\tLine "+str(line)+"\n"
                    "\tOpenGL Error: "+errors;

            error_cgp(msg);
        }
	}
}