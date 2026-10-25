#include "triangles_drawable.hpp"

#include "cgp/01_base/base.hpp"

#if defined(__linux__) || defined(__EMSCRIPTEN__)
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

namespace cgp
{
	opengl_shader_structure triangles_drawable::default_shader;
	opengl_texture_image_structure triangles_drawable::default_texture;

	static void warning_initialize_non_empty();

	void triangles_drawable::initialize_data_on_gpu(numarray<vec3> const& position, numarray<vec3> const& normal, numarray<vec3> const& color, numarray<vec3> const& uv,opengl_shader_structure const& shader_arg, opengl_texture_image_structure const& texture_arg)
	{
		// Error detection before sending the data to avoid unexpected behavior
		// *********************************************************************** //

		opengl_check;

		// Check if this mesh_drawable is already initialized
		if (vao != 0 || vbo_position.size != 0)
			warning_initialize_non_empty();

		if (position.size() == 0) {
			warning_cgp("Warning try to generate triangles_drawable with 0 vertex", "");
			return;
		}


		// Variable initialization
		// *********************************************************************** //

		shader = shader_arg;
		texture = texture_arg;
		model = affine();
		material = material_mesh_drawable_phong();


		// Send the data to the GPU
		// ******************************************** //

		vbo_position.initialize_data_on_gpu(position);
		vertex_number = position.size();
		
		int const N_vertex = position.size();
		if (normal.size() < N_vertex) {
			numarray<vec3> n; n.resize(N_vertex);
			for (int k = 2; k < N_vertex; k+=3) {
				vec3 const& p0 = position[k - 2];
				vec3 const& p1 = position[k - 1];
				vec3 const& p2 = position[k];

				vec3 const u1 = p1 - p0;
				vec3 const u2 = p2 - p0;

				vec3 n_tri = cross(u1, u2);
				float const n_length = norm(n_tri);
				if (n_length < 1e-4f)
					n_tri = { 1,0,0 };

				n[k]   = n_tri;
				n[k-1] = n_tri;
				n[k-2] = n_tri;
			}
			vbo_normal.initialize_data_on_gpu(n); // upload the auto-computed normals
		}
		else {
			vbo_normal.initialize_data_on_gpu(normal);
		}

		if (color.size() < N_vertex) {
			numarray<vec3> v; v.resize(N_vertex);
			v.fill({ 1,1,1 });
			vbo_color.initialize_data_on_gpu(v);
		}
		else {
			vbo_color.initialize_data_on_gpu(color);
		}

		if (uv.size() < N_vertex) {
			numarray<vec2> v; v.resize(N_vertex);
			v.fill({ 0,0 });
			vbo_uv.initialize_data_on_gpu(v);
		}
		else{
			vbo_uv.initialize_data_on_gpu(uv);
		}

		
		// Generate VAO
		glGenVertexArrays(1, &vao); opengl_check;
		glBindVertexArray(vao); opengl_check;
		opengl_set_vao_location(vbo_position, 0);
		opengl_set_vao_location(vbo_normal, 1);
		opengl_set_vao_location(vbo_color, 2);
		opengl_set_vao_location(vbo_uv, 3);
		glBindVertexArray(0); opengl_check;
	}

	void triangles_drawable::clear()
	{
		vbo_position.clear();
		vbo_normal.clear();
		vbo_color.clear();
		vbo_uv.clear();
		
		if(vao!=0)
			glDeleteVertexArrays(1, &vao);
		vao = 0;
		vertex_number = 0;

		shader = opengl_shader_structure();
		model = affine();
		material = material_mesh_drawable_phong();
		texture = opengl_texture_image_structure();
		supplementary_texture.clear();

		opengl_check;
	}


	static void warning_initialize_non_empty()
	{
		std::string warning = "\n";
		warning += "  > You are calling initialize_gpu_data on an non-empty triangles_drawable \n";
		warning += "In normal condition, you should avoid initializing triangles_drawable on an existing one without clearing it - the previously allocated memory on the GPU is going to be lost.\n";
		warning += " - If you want to clear the memory, please call triangles_drawable.clear() before calling a new initialization\n";
		warning += " - Note that you should generally not call initialize_data_on_gpu() in the animation loop\n";

		warning_cgp("Calling initialize_data_on_gpu() on a triangles_drawable with non zero VBOs", warning);
	}


	void draw(triangles_drawable const& drawable, environment_generic_structure const& environment, uniform_generic_structure const& additional_uniforms, material_mesh_drawable_phong const* material_override)
	{
		// Initial clean check
		// ********************************** //
		// If there is not vertices or not triangles, returns
		//  (no error + does not display anything)
		if (drawable.vbo_position.size == 0)
			return;

		assert_cgp(drawable.shader.id != 0, "Try to draw mesh_drawable without shader ");
		assert_cgp(glIsProgram(drawable.shader.id), "Try to draw triangles_drawable with incorrect shader ");
		assert_cgp(drawable.texture.id != 0, "Try to draw mesh_drawable without texture ");

		// Set the current shader
		// ********************************** //
		glUseProgram(drawable.shader.id); opengl_check;

		// Send uniforms for this shader
		// ********************************** //

		// send the uniform values for the model and material of the triangles_drawable
		drawable.send_opengl_uniform();

		// optional material override (e.g. for the wireframe pass): re-sent after, so it wins
		if (material_override != nullptr)
			material_override->send_opengl_uniform(drawable.shader);

		// send the uniform values for the environment
		environment.send_opengl_uniform(drawable.shader);

		// [Optionnal] send any additional uniform for this specidic draw call
		additional_uniforms.send_opengl_uniform(drawable.shader);


		// Set textures
		// ********************************** //
		glActiveTexture(GL_TEXTURE0); opengl_check;
		drawable.texture.bind();
		opengl_uniform(drawable.shader, "image_texture", 0);  opengl_check;

		//Set any additional texture
		int texture_count = 1;
		for (auto const& element : drawable.supplementary_texture)
		{
			std::string const& additional_texture_name = element.first;
			opengl_texture_image_structure const& additional_texture = element.second;

			glActiveTexture(GL_TEXTURE0 + texture_count); opengl_check;
			additional_texture.bind();
			opengl_uniform(drawable.shader, additional_texture_name, texture_count);

			texture_count++;
		}


		// Prepare for draw call
		// ********************************** //
		glBindVertexArray(drawable.vao);                                     opengl_check;


		// Draw call
		// ********************************** //
		glDrawArrays(GL_TRIANGLES, 0, drawable.vertex_number); opengl_check;


		// Clean state
		// ********************************** //
		glBindVertexArray(0);

		// Unbind every texture unit that was bound, and restore the active unit to GL_TEXTURE0.
		int texture_unit_cleanup = 1;
		for (auto const& element : drawable.supplementary_texture) {
			glActiveTexture(GL_TEXTURE0 + texture_unit_cleanup); opengl_check;
			element.second.unbind();
			texture_unit_cleanup++;
		}
		glActiveTexture(GL_TEXTURE0); opengl_check;
		drawable.texture.unbind();

		glUseProgram(0);
	}

	void draw_wireframe(triangles_drawable const& drawable, environment_generic_structure const& environment, vec3 const& color, uniform_generic_structure const& additional_uniforms)
	{
		#ifndef __EMSCRIPTEN__
		// Wireframe material (flat color, no texture). Passed by pointer to draw() so it
		//  reuses the same drawable - no copy of its textures / supplementary VBOs per call.
		material_mesh_drawable_phong wireframe_material;
		wireframe_material.phong = { 1.0f,0.0f,0.0f,64.0f };
		wireframe_material.color = color;
		wireframe_material.texture_settings.active = false;
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		glEnable(GL_POLYGON_OFFSET_LINE);
		glPolygonOffset(-1.0, 1.0);        opengl_check;
		draw(drawable, environment, additional_uniforms, &wireframe_material);
		glDisable(GL_POLYGON_OFFSET_LINE); opengl_check;
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		#endif
	}


	void triangles_drawable::send_opengl_uniform(bool expected) const
	{
		// Final model matrix in the shader is: hierarchy_transform_model * model
		mat4 const model_shader = hierarchy_transform_model.matrix() * model.matrix();

		// set the Model matrix
		//  Note: the normal matrix transpose(inverse(model)) is derived in the vertex shader
		//  (consistent with mesh_drawable), so no modelNormal uniform is sent from here.
		opengl_uniform(shader, "model", model_shader, expected);

		// set the material
		material.send_opengl_uniform(shader);
	}
}