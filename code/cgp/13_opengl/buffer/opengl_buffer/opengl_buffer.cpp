#include "opengl_buffer.hpp"
#include "../../debug/debug.hpp"


namespace cgp
{
	void opengl_gpu_buffer::bind() const
	{
		if(id!=0)
			glBindBuffer(type, id);
	}
	void opengl_gpu_buffer::unbind() const
	{
		glBindBuffer(type, 0);
	}
	void opengl_gpu_buffer::clear()
	{
		glDeleteBuffers(1, &id);  opengl_check;

		id = 0;
		size = 0;
		type = 0;
		details = opengl_gpu_buffer_details();		
	}

}