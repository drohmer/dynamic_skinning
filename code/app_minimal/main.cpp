#include "cgp/cgp.hpp"
#include "environment.hpp"
#include "scene.hpp"

// Generic window and display loop of the cgp library (the dynamic skinning is in scene.cpp)

scene_structure scene;

void window_size_callback(GLFWwindow*, int width, int height);
void mouse_move_callback(GLFWwindow*, double xpos, double ypos);
void mouse_click_callback(GLFWwindow* window, int button, int action, int mods);
void mouse_scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void keyboard_callback(GLFWwindow* window, int key, int scancode, int action, int mods);


int main(int, char* argv[])
{
	// Window, ImGui and inputs
	scene.window.initialize_glfw();
	scene.window.create_window(1280, 960, "Dynamic Skinning - minimal", CGP_OPENGL_VERSION_MAJOR, CGP_OPENGL_VERSION_MINOR);
	cgp::imgui_init(scene.window.glfw_window);
	glfwSetWindowSizeCallback(scene.window.glfw_window, window_size_callback);
	glfwSetCursorPosCallback(scene.window.glfw_window, mouse_move_callback);
	glfwSetMouseButtonCallback(scene.window.glfw_window, mouse_click_callback);
	glfwSetScrollCallback(scene.window.glfw_window, mouse_scroll_callback);
	glfwSetKeyCallback(scene.window.glfw_window, keyboard_callback);

	// Shaders and assets are in the root directory of the project
	project::path = cgp::project_path_find(argv[0], "shaders/");
	std::string const shaders = project::path + "shaders/";
	mesh_drawable::default_shader.load(shaders + "mesh/mesh.vert.glsl", shaders + "mesh/mesh.frag.glsl");
	mesh_drawable::default_texture.initialize_texture_2d_on_gpu(image_structure{1, 1, image_color_type::rgba, {255, 255, 255, 255}});

	scene.initialize();

	while (!glfwWindowShouldClose(scene.window.glfw_window)) {
		scene.inputs.time_interval = 1 / 60.0f;
		scene.camera_projection.aspect_ratio = scene.window.aspect_ratio();
		scene.environment.camera_projection = scene.camera_projection.matrix();
		glViewport(0, 0, scene.window.width, scene.window.height);
		vec3 const& background = scene.environment.background_color;
		glClearColor(background.x, background.y, background.z, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glEnable(GL_DEPTH_TEST);

		cgp::imgui_create_frame();
		scene.inputs.mouse.on_gui = ImGui::GetIO().WantCaptureMouse;
		scene.display_gui();
		scene.idle_frame();
		scene.display_frame();
		cgp::imgui_render_frame(scene.window.glfw_window);

		glfwSwapBuffers(scene.window.glfw_window);
		glfwPollEvents();
	}

	cgp::imgui_cleanup();
	glfwDestroyWindow(scene.window.glfw_window);
	glfwTerminate();
	return 0;
}


void window_size_callback(GLFWwindow*, int width, int height)
{
	scene.window.width = width;
	scene.window.height = height;
}

void mouse_move_callback(GLFWwindow*, double xpos, double ypos)
{
	scene.inputs.mouse.position.update(scene.window.convert_pixel_to_relative_coordinates({xpos, ypos}));
	scene.mouse_move_event();
}

void mouse_click_callback(GLFWwindow* window, int button, int action, int mods)
{
	ImGui_ImplGlfw_MouseButtonCallback(window, button, action, mods);
	scene.inputs.mouse.click.update_from_glfw_click(button, action);
	scene.mouse_click_event();
}

void mouse_scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	ImGui_ImplGlfw_ScrollCallback(window, xoffset, yoffset);
	scene.inputs.mouse.scroll = yoffset;
	scene.mouse_scroll_event();
}

void keyboard_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	ImGui_ImplGlfw_KeyCallback(window, key, scancode, action, mods);
	if (!ImGui::GetIO().WantCaptureKeyboard) {
		scene.inputs.keyboard.update_from_glfw_key(key, action);
		scene.keyboard_event();
	}
}
