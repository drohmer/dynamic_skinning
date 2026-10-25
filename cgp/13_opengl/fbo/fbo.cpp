#include "fbo.hpp"

#include "cgp/01_base/base.hpp"


namespace cgp{

	static std::string framebuffer_status_to_string(GLenum status)
	{
		switch (status)
		{
		case GL_FRAMEBUFFER_COMPLETE:                      return "GL_FRAMEBUFFER_COMPLETE";
		case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:         return "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT";
		case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT: return "GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT";
		case GL_FRAMEBUFFER_UNSUPPORTED:                   return "GL_FRAMEBUFFER_UNSUPPORTED";
		// Note: GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS is not part of GL 3.3 core nor GLES3,
		//  so it is reported through the default branch.
		default:                                           return "UNKNOWN (" + str(int(status)) + ")";
		}
	}

	void opengl_fbo_structure::initialize() {

		width = 800;
		height = 800;

		if(mode == opengl_fbo_mode::image) {

			// Initialize texture
			texture.initialize_texture_2d_on_gpu(width, height, GL_RGB8, GL_TEXTURE_2D);

			// Allocate a depth buffer - need to do it when using the frame buffer
			glGenRenderbuffers(1, &depth_buffer_id); opengl_check;
			glBindRenderbuffer(GL_RENDERBUFFER, depth_buffer_id); opengl_check;
			glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT32F, width, height); opengl_check;
			glBindRenderbuffer(GL_RENDERBUFFER, 0); opengl_check;

			// Create frame buffer
			glGenFramebuffers(1, &id);
			glBindFramebuffer(GL_FRAMEBUFFER, id);
			// associate the texture to the frame buffer
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture.id, 0);
			// associate the depth-buffer
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth_buffer_id);
		}
		else if(mode == opengl_fbo_mode::depth) {

			// Initialize texture
			texture.width = width;
			texture.height = height;
			texture.format = GL_DEPTH_COMPONENT;
			texture.texture_type = GL_TEXTURE_2D;
			glGenTextures(1, &texture.id); opengl_check;
			glBindTexture(GL_TEXTURE_2D, texture.id); opengl_check;
			glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr); opengl_check;
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST); opengl_check;
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); opengl_check;
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); opengl_check;
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); opengl_check;
			glBindTexture(GL_TEXTURE_2D, 0); opengl_check;

			

			//float borderColor[] = { 1.0, 1.0, 1.0, 1.0 }; opengl_check;
			//glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor); opengl_check;

			// Create frame buffer
			glGenFramebuffers(1, &id); opengl_check;
			glBindFramebuffer(GL_FRAMEBUFFER, id); opengl_check;
			// associate the texture to the frame buffer
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, texture.id, 0); opengl_check;

			// No color buffer is used in depth mode
			#ifndef __EMSCRIPTEN__
			glDrawBuffer(GL_NONE);
			glReadBuffer(GL_NONE);
			#else
			glGenTextures(1, &emscripten_color_attachment_id); opengl_check;
			glBindTexture(GL_TEXTURE_2D, emscripten_color_attachment_id); opengl_check;
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr); opengl_check;
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); opengl_check;
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); opengl_check;
			glBindTexture(GL_TEXTURE_2D, 0); opengl_check;
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, emscripten_color_attachment_id, 0); opengl_check;
			#endif
		}

		// Verify the framebuffer is complete (still bound at this point)
		GLenum const status = glCheckFramebufferStatus(GL_FRAMEBUFFER); opengl_check;
		if (status != GL_FRAMEBUFFER_COMPLETE)
			error_cgp("Framebuffer incomplete after initialize(): " + framebuffer_status_to_string(status));

		// Reset the standard framebuffer to output on the screen
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	void opengl_fbo_structure::bind() const {
		// Save the current viewport so unbind() can restore it.
		glGetIntegerv(GL_VIEWPORT, viewport_backup); opengl_check;
		glBindFramebuffer(GL_FRAMEBUFFER, id); opengl_check;
		glViewport(0, 0, width, height); opengl_check;
		// Note: the result is stored in the [texture] member; sample it via fbo.texture.bind()
		//  after unbind(). The render-target texture is intentionally NOT bound here.
	}
	
	void opengl_fbo_structure::unbind() const {
		glBindFramebuffer(GL_FRAMEBUFFER, 0); opengl_check;
		glViewport(viewport_backup[0], viewport_backup[1], viewport_backup[2], viewport_backup[3]); opengl_check;
	}



	void opengl_fbo_structure::update_screen_size(int new_width, int new_height) {

		if (width != new_width || height != new_height) 
		{
			width = new_width;
			height = new_height;

			if(mode==opengl_fbo_mode::image){
				glBindTexture(GL_TEXTURE_2D, texture.id);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
				glBindTexture(GL_TEXTURE_2D, 0);

				glBindRenderbuffer(GL_RENDERBUFFER, depth_buffer_id);
				glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT32F, width, height);
				glBindRenderbuffer(GL_RENDERBUFFER, 0);
				opengl_check;
			}
			else if(mode==opengl_fbo_mode::depth){
				glBindTexture(GL_TEXTURE_2D, texture.id);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, NULL);
				glBindTexture(GL_TEXTURE_2D, 0);
				opengl_check;
			}

			
		}
		

	}

}