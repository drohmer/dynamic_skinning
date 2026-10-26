

#include "cgp/cgp.hpp" // Give access to the complete CGP library
#include "environment.hpp" // The general scene environment + project variable
#include <iostream> 

#include <chrono>
#include <thread>
#include <sstream>
#include <cstdlib>
#include <algorithm>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

// Custom scene of this code
#include "scene.hpp"
#include "capture/frame_capture.hpp"




// *************************** //
// Custom Scene defined in "scene.hpp"
// *************************** //

scene_structure scene;

// Automatic screenshots (enabled with the DS_CAPTURE_DIR environment variable)
frame_capture_structure capture;


// The rest of this code is a generic initialization and animation loop that can be applied to different scenes
// *************************** //
// Start of the program
// *************************** //

window_structure standard_window_initialization();
void initialize_default_shaders();
void animation_loop();
void display_gui_default();

timer_fps fps_record;

// Scale at which the GUI font is rasterized (1: default font, enlarged by FontGlobalScale)
float gui_font_scale = 1.0f;

#ifdef __EMSCRIPTEN__
// Number of device pixels per CSS pixel used for the canvas: devicePixelRatio (2 on Retina, tablets, 3 on
// phones), limited to 2 to keep the frame rate on mobile GPUs
float web_pixel_ratio()
{
	return float(EM_ASM_DOUBLE({ return Math.min(window.devicePixelRatio || 1, 2); }));
}
#endif

int main(int, char* argv[])
{
	std::cout << "Run " << argv[0] << std::endl;

#ifdef __EMSCRIPTEN__
	// The browsers of iOS have no Pointer Lock API: the GLFW of Emscripten calls document.exitPointerLock when
	// ImGui sets the cursor mode (each frame), which throws an exception and stops the application. The missing
	// functions are replaced by functions doing nothing.
	EM_ASM({
		if (!document.exitPointerLock) document.exitPointerLock = function() {};
		if (!HTMLCanvasElement.prototype.requestPointerLock) HTMLCanvasElement.prototype.requestPointerLock = function() {};
	});
#endif

	

	// ************************ //
	//     INITIALISATION
	// ************************ //
	
	// Standard Initialization of an OpenGL ready window
	scene.window = standard_window_initialization();

#ifdef __EMSCRIPTEN__
	// Touch screen (phones, tablets): larger GUI, so that the widgets can be used with a finger, limited by the size
	// of the screen (the GUI is about 260 points wide at scale 1): 1.2 on a phone, 1.6 on a tablet
	if (EM_ASM_INT({ return navigator.maxTouchPoints > 0 ? 1 : 0; })) {
		float const screen_size = float(EM_ASM_DOUBLE({ return Math.min(window.innerWidth, window.innerHeight); }));
		project::gui_scale = std::clamp(screen_size / 320.0f, 1.0f, 1.6f);
		ImGui::GetIO().ConfigWindowsMoveFromTitleBarOnly = true;  // no accidental move of the windows
		// Skinning on the CPU by default: faster than the vertex shader on mobile GPUs (see scene.hpp)
		scene.gpu_skinning_default = false;
		scene.use_gpu_skinning = false;
		// Gestures without mode: a finger on a joint rotates it, elsewhere it orbits the camera (see scene.hpp)
		scene.touch_interaction = true;
	}
	// The canvas is in device pixels (see animation_loop): the GUI is scaled by the same pixel ratio, and its font
	// is rasterized at its final size, so that the text is sharp instead of an enlarged bitmap
	project::gui_scale *= web_pixel_ratio();
	ImGui::GetStyle().ScaleAllSizes(project::gui_scale);
	{
		ImFontConfig font_config;
		font_config.SizePixels = std::round(13.0f * project::gui_scale);
		ImGui::GetIO().Fonts->AddFontDefault(&font_config);
		gui_font_scale = project::gui_scale;
	}
#endif

	// Initialize default path for assets
	project::path = cgp::project_path_find(argv[0], "shaders/");

	// Initialize default shaders
	initialize_default_shaders();


	// Custom scene initialization
	std::cout << "Initialize data of the scene ..." << std::endl;
	scene.initialize();
	std::cout << "Initialization finished\n" << std::endl;

	capture.initialize_from_environment();
	if (capture.active) {
		if (!capture.model.empty())
			scene.load_model(capture.model);
		if (capture.has_eye) {
			auto& cam = scene.camera_control.camera_model;
			cam.look_at(capture.eye, cam.center_of_rotation, cam.orientation() * vec3{0, 1, 0});  // keeps the up vector of the preset
		}
		if (capture.amplitude >= 0)
			scene.ds_params.amplitude = capture.amplitude;
		if (capture.drag >= 0)
			scene.ds_params.drag = capture.drag;
		if (capture.zoom != 1.0f)
			scene.camera_control.camera_model.distance_to_center *= capture.zoom;
		if (capture.gpu >= 0)
			scene.use_gpu_skinning = (capture.gpu == 1);
		if (capture.dynamic_skinning >= 0)
			scene.ds_params.active = (capture.dynamic_skinning == 1);
		if (capture.skeleton >= 0) {
			scene.gui.display_skeleton_bones = (capture.skeleton == 1);
			scene.gui.display_skeleton_joints = (capture.skeleton == 1);
		}
		if (capture.delay >= 0)
			scene.ds_params.delay_active = (capture.delay == 1);
		if (capture.fps > 0) {
			project::vsync = false;
			glfwSwapInterval(0);
			project::fps_limiting = true;
			project::fps_max = capture.fps;
		}
		scene.character.timer.start();
		scene.ds_timer.start();  // both timers restart on the clock of the capture (virtual clock in video mode)
		scene.animation_running = true;
		scene.show_settings = (capture.gui == 2);
	}


	// ************************ //
	//     Animation Loop
	// ************************ //
	std::cout << "Start animation loop ..." << std::endl;
	fps_record.start();


	// Call the main display loop in the function animation_loop
	//  The following part is simply a loop that call the function "animation_loop"
	//  (This call is different when we compile in standard mode with GLFW, than when we compile with emscripten to output the result in a webpage.)
#ifndef __EMSCRIPTEN__
    double lasttime = glfwGetTime();
	// Default mode to run the animation/display loop with GLFW in C++
	while (!glfwWindowShouldClose(scene.window.glfw_window)) {
		// The real animation loop
		animation_loop();

		// FPS limitation
		if(project::fps_limiting){
			while (glfwGetTime() < lasttime + 1.0 / project::fps_max) {	}
        	lasttime = glfwGetTime();
		}
	}
#else
	// Specific loop if compiled for EMScripten
	emscripten_set_main_loop(animation_loop, 0, 1);
#endif

	std::cout << "\nAnimation loop stopped" << std::endl;

	// Cleanup
	cgp::imgui_cleanup();
	glfwDestroyWindow(scene.window.glfw_window);
	glfwTerminate();

	return 0;
}

void animation_loop()
{

#ifdef __EMSCRIPTEN__
	// The canvas follows its size on the page (set by the CSS), in device pixels: sharp image on high density
	// screens. Resizing it through GLFW keeps the mouse/touch coordinates in the same pixels as the canvas.
	{
		float const ratio = web_pixel_ratio();
		int const width = int(std::round(ratio * EM_ASM_INT({ return document.getElementById('canvas').clientWidth; })));
		int const height = int(std::round(ratio * EM_ASM_INT({ return document.getElementById('canvas').clientHeight; })));
		int current_width = 0, current_height = 0;
		glfwGetWindowSize(scene.window.glfw_window, &current_width, &current_height);
		if (width > 0 && height > 0 && (width != current_width || height != current_height))
			glfwSetWindowSize(scene.window.glfw_window, width, height);
	}
#endif
	emscripten_update_window_size(scene.window.width, scene.window.height); // update window size in case of use of emscripten (not used by default)

	// Narrow window (portrait phone or tablet): the camera presets of the models are set for a landscape window
	// (aspect ratio about 1.5). The vertical field of view is widened, so that the horizontal one stays the one
	// of that window and the character is not cut on the sides.
	static float const field_of_view_reference = scene.camera_projection.field_of_view;
	float const aspect_ratio_reference = 1.5f;
	float const aspect_ratio = scene.window.aspect_ratio();
	scene.camera_projection.field_of_view = aspect_ratio >= aspect_ratio_reference ? field_of_view_reference :
		2.0f * std::atan(std::tan(field_of_view_reference / 2.0f) * aspect_ratio_reference / aspect_ratio);
	scene.camera_projection.aspect_ratio = aspect_ratio;
	scene.environment.camera_projection = scene.camera_projection.matrix();
	glViewport(0, 0, scene.window.width, scene.window.height);

	vec3 const& background_color = scene.environment.background_color;
	glClearColor(background_color.x, background_color.y, background_color.z, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	glClear(GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);

	float const time_interval = fps_record.update();
#ifndef __EMSCRIPTEN__
	// Frame rate in the window title (not in the web version, where it would replace the title of the page)
	if (fps_record.event) {
		std::string const title = "CGP Display - " + str(fps_record.fps) + " fps";
		glfwSetWindowTitle(scene.window.glfw_window, title.c_str());
	}
#endif

	imgui_create_frame();
	ImGui::GetIO().FontGlobalScale = project::gui_scale / gui_font_scale;
	scene.inputs.mouse.on_gui = ImGui::GetIO().WantCaptureMouse;
	scene.inputs.time_interval = time_interval;


	// Display the ImGUI interface: main toolbar, and the advanced settings in a separate window
	scene.display_gui();
	if (scene.show_settings) {
		ImGui::SetNextWindowPos(ImVec2(10, 130 * project::gui_scale), ImGuiCond_FirstUseEver);
		ImGui::Begin("Settings", &scene.show_settings, ImGuiWindowFlags_AlwaysAutoResize);
		display_gui_default();
		scene.display_gui_settings();
		ImGui::End();
	}

	// Handle camera behavior in standard frame
	scene.idle_frame();

	// Call the display of the scene
	scene.display_frame();

	// Save a screenshot (before the GUI is drawn, unless requested) when in capture mode
	if (capture.gui == 0)
		capture.update(scene.character.timer.t, scene.current_model_name);


	// End of ImGui display and handle GLFW events
	imgui_render_frame(scene.window.glfw_window);
	if (capture.gui > 0)
		capture.update(scene.character.timer.t, scene.current_model_name);
	if (capture.finished())
		glfwSetWindowShouldClose(scene.window.glfw_window, true);
	glfwSwapBuffers(scene.window.glfw_window);
	glfwPollEvents();
}


void initialize_default_shaders()
{
	// Generate the default directory from which the shaders are found
	//  By default, it should be "shaders/"
	std::string default_path_shaders = project::path +"shaders/";

	// Set standard mesh shader for mesh_drawable
	mesh_drawable::default_shader.load(default_path_shaders +"mesh/mesh.vert.glsl", default_path_shaders +"mesh/mesh.frag.glsl");
	triangles_drawable::default_shader.load(default_path_shaders +"mesh/mesh.vert.glsl", default_path_shaders +"mesh/mesh.frag.glsl");

	// Set default white texture
	image_structure const white_image = image_structure{ 1,1,image_color_type::rgba,{255,255,255,255} };
	mesh_drawable::default_texture.initialize_texture_2d_on_gpu(white_image);
	triangles_drawable::default_texture.initialize_texture_2d_on_gpu(white_image);

	// Set standard uniform color for curve/segment_drawable
	curve_drawable::default_shader.load(default_path_shaders +"single_color/single_color.vert.glsl", default_path_shaders+"single_color/single_color.frag.glsl");
}





//Callback functions
void window_size_callback(GLFWwindow* window, int width, int height);
void mouse_move_callback(GLFWwindow* window, double xpos, double ypos);
void mouse_scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void mouse_click_callback(GLFWwindow* window, int button, int action, int mods);
void keyboard_callback(GLFWwindow* window, int key, int, int action, int mods);

// Standard initialization procedure
window_structure standard_window_initialization()
{
	// Initialize GLFW and create window
	// ***************************************************** //

	// First initialize GLFW
	scene.window.initialize_glfw();

	// Compute initial window width and height
	int window_width = int(project::initial_window_size_width);
	int window_height = int(project::initial_window_size_height);
	if(project::initial_window_size_width<1)
		window_width = project::initial_window_size_width * scene.window.monitor_width();
	if(project::initial_window_size_height<1)
		window_height = project::initial_window_size_height * scene.window.monitor_height();

	// Window size imposed by the capture mode (DS_CAPTURE_SIZE="width,height")
	if (char const* size_env = std::getenv("DS_CAPTURE_SIZE")) {
		char comma;
		std::stringstream ss(size_env);
		ss >> window_width >> comma >> window_height;
	}

	// Create the window using GLFW
	window_structure window;
	window.create_window(window_width, window_height, "CGP Display", CGP_OPENGL_VERSION_MAJOR, CGP_OPENGL_VERSION_MINOR);


	// Display information
	// ***************************************************** //

	// Display window size
	std::cout << "\nWindow (" << window.width << "px x " << window.height << "px) created" << std::endl;
	std::cout << "Monitor: " << glfwGetMonitorName(window.monitor) << " - Resolution (" << window.screen_resolution_width << "x" << window.screen_resolution_height << ")\n" << std::endl;

	// Display debug information on command line
	std::cout << "OpenGL Information:" << std::endl;
	std::cout << cgp::opengl_info_display() << std::endl;

	// Initialize ImGUI
	// ***************************************************** //
	cgp::imgui_init(window.glfw_window);

	// Set the callback functions for the inputs
	glfwSetMouseButtonCallback(window.glfw_window, mouse_click_callback); // Event called when a button of the mouse is clicked/released
	glfwSetCursorPosCallback(window.glfw_window, mouse_move_callback);    // Event called when the mouse is moved
	glfwSetWindowSizeCallback(window.glfw_window, window_size_callback);  // Event called when the window is rescaled        
	glfwSetKeyCallback(window.glfw_window, keyboard_callback);            // Event called when a keyboard touch is pressed/released
	glfwSetScrollCallback(window.glfw_window, mouse_scroll_callback);     // Event called when scrolling the mouse

	return window;
}




// This function is called everytime the window is resized
void window_size_callback(GLFWwindow*, int width, int height)
{
	scene.window.width = width;
	scene.window.height = height;
}

// This function is called everytime the mouse is moved
void mouse_move_callback(GLFWwindow* window, double xpos, double ypos)
{
	// ImGui receives the position before the clicks (with a touch screen, the finger moves the cursor just before the click)
	ImGui_ImplGlfw_CursorPosCallback(window, xpos, ypos);

	vec2 const pos_relative = scene.window.convert_pixel_to_relative_coordinates({ xpos, ypos });
	scene.inputs.mouse.position.update(pos_relative);
	scene.mouse_move_event();
}

// This function is called everytime a mouse button is clicked/released
void mouse_click_callback(GLFWwindow* window, int button, int action, int mods)
{
#ifdef __EMSCRIPTEN__
	// With a touch screen, the press of the finger arrives before its position (sent only when the finger moves):
	// the position is sent first, otherwise ImGui and the scene would apply the press at the previous position
	if (action == GLFW_PRESS) {
		double xpos = 0, ypos = 0;
		glfwGetCursorPos(window, &xpos, &ypos);
		mouse_move_callback(window, xpos, ypos);
	}
#endif
	ImGui_ImplGlfw_MouseButtonCallback(window, button, action, mods);
	
	scene.inputs.mouse.click.update_from_glfw_click(button, action);
	scene.mouse_click_event();
}

// This function is called everytime the mouse is scrolled
void mouse_scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	ImGui_ImplGlfw_ScrollCallback(window, xoffset, yoffset);

	scene.inputs.mouse.scroll = yoffset;
	scene.mouse_scroll_event();
}

// This function is called everytime a keyboard touch is pressed/released
void keyboard_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	ImGui_ImplGlfw_KeyCallback(window, key, scancode, action, mods);
	bool imgui_capture_keyboard = ImGui::GetIO().WantCaptureKeyboard;
	
	if(!imgui_capture_keyboard){
		scene.inputs.keyboard.update_from_glfw_key(key, action);
		scene.keyboard_event();

		// Press 'F' for full screen mode
		if (key == GLFW_KEY_F && action == GLFW_PRESS && scene.inputs.keyboard.shift) {
			scene.window.is_full_screen = !scene.window.is_full_screen;
			if (scene.window.is_full_screen)
				scene.window.set_full_screen();
			else
				scene.window.set_windowed_screen();
		}
		// Press 'V' for camera frame/view matrix debug
		if (key == GLFW_KEY_V && action == GLFW_PRESS && scene.inputs.keyboard.shift) {
			auto const camera_model = scene.camera_control.camera_model;
			std::cout << "\nDebug camera (position = " << str(camera_model.position()) << "):\n" << std::endl;
			std::cout << "  Frame matrix:" << std::endl;
			std::cout << str_pretty(camera_model.matrix_frame()) << std::endl;
			std::cout << "  View matrix:" << std::endl;
			std::cout << str_pretty(camera_model.matrix_view()) << std::endl;

		}
	}

}

void display_gui_default()
{
	std::string fps_txt = str(fps_record.fps)+" fps";

	if(scene.inputs.keyboard.ctrl)
		fps_txt += " [ctrl]";
	if(scene.inputs.keyboard.shift)
		fps_txt += " [shift]";

	ImGui::Text( fps_txt.c_str(), "%s" );
	if(ImGui::CollapsingHeader("Window")) {
		ImGui::Indent();
#ifndef __EMSCRIPTEN__
		bool changed_screen_mode = ImGui::Checkbox("Full Screen", &scene.window.is_full_screen);
		if(changed_screen_mode){	
			if (scene.window.is_full_screen)
				scene.window.set_full_screen();
			else
				scene.window.set_windowed_screen();
		}
#endif
		ImGui::SliderFloat("Gui Scale", &project::gui_scale, 0.5f, 2.5f);

#ifndef __EMSCRIPTEN__
		// Arbitrary limits the refresh rate to a maximal frame per seconds.
		//  This limits the risk of having different behaviors when you use different machine. 
		ImGui::Checkbox("FPS limiting",&project::fps_limiting);
		if(project::fps_limiting){
			ImGui::SliderFloat("FPS limit",&project::fps_max, 10, 250);
		}
#endif
		// vsync is the default synchronization of frame refresh with the screen frequency
		//   vsync may or may not be enforced by your GPU driver and OS (on top of the GLFW request).
		//   de-activating vsync may generate arbitrary large FPS depending on your GPU and scene.
		if(ImGui::Checkbox("vsync (screen sync)",&project::vsync)){
			project::vsync==true? glfwSwapInterval(1) : glfwSwapInterval(0); 
		}

		std::string window_size = "Window "+str(scene.window.width)+"px x "+str(scene.window.height)+"px";
		ImGui::Text( window_size.c_str(), "%s" );

		ImGui::Unindent();


		ImGui::Spacing();ImGui::Separator();ImGui::Spacing();
	}
}


