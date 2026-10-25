#include "frame_capture.hpp"

#include <cstdlib>
#include <sstream>
#include <iomanip>
#include <cmath>

using namespace cgp;


// Virtual clock of the video mode, advanced of 1/fps after each saved frame
static double virtual_time = 0.0;
static double virtual_time_source() { return virtual_time; }


void frame_capture_structure::initialize_from_environment()
{
    char const* dir = std::getenv("DS_CAPTURE_DIR");
    if (dir == nullptr)
        return;

    active = true;
    directory = std::string(dir) + "/";

    char const* model_env = std::getenv("DS_CAPTURE_MODEL");
    if (model_env != nullptr)
        model = model_env;

    char const* ds_env = std::getenv("DS_CAPTURE_DS");
    if (ds_env != nullptr)
        dynamic_skinning = std::atoi(ds_env);

    char const* delay_env = std::getenv("DS_CAPTURE_DELAY");
    if (delay_env != nullptr)
        delay = std::atoi(delay_env);

    char const* fps_env = std::getenv("DS_CAPTURE_FPS");
    if (fps_env != nullptr)
        fps = float(std::atof(fps_env));

    char const* skeleton_env = std::getenv("DS_CAPTURE_SKELETON");
    if (skeleton_env != nullptr)
        skeleton = std::atoi(skeleton_env);

    char const* gpu_env = std::getenv("DS_CAPTURE_GPU");
    if (gpu_env != nullptr)
        gpu = std::atoi(gpu_env);

    char const* gui_env = std::getenv("DS_CAPTURE_GUI");
    if (gui_env != nullptr)
        gui = std::atoi(gui_env);

    char const* amplitude_env = std::getenv("DS_CAPTURE_AMPLITUDE");
    if (amplitude_env != nullptr)
        amplitude = float(std::atof(amplitude_env));

    char const* drag_env = std::getenv("DS_CAPTURE_DRAG");
    if (drag_env != nullptr)
        drag = float(std::atof(drag_env));

    char const* zoom_env = std::getenv("DS_CAPTURE_ZOOM");
    if (zoom_env != nullptr)
        zoom = float(std::atof(zoom_env));

    char const* eye_env = std::getenv("DS_CAPTURE_EYE");
    if (eye_env != nullptr) {
        std::stringstream se(eye_env);
        char comma;
        se >> eye.x >> comma >> eye.y >> comma >> eye.z;
        has_eye = true;
    }

    times.clear();
    char const* video_env = std::getenv("DS_CAPTURE_VIDEO");
    if (video_env != nullptr) {
        float duration = 0;
        char comma;
        std::stringstream sv(video_env);
        sv >> video_fps >> comma >> duration;
        int const frame_count = int(std::round(video_fps * duration));
        for (int k = 0; k < frame_count; ++k)
            times.push_back(float(k) / video_fps);
        timer_set_time_source(virtual_time_source);
        std::cout << "Capture mode: video " << video_fps << " fps, " << frame_count << " frame(s) -> " << directory << std::endl;
        return;
    }

    char const* times_env = std::getenv("DS_CAPTURE_TIMES");
    std::stringstream ss(times_env != nullptr ? times_env : "0");
    std::string item;
    while (std::getline(ss, item, ','))
        if (!item.empty())
            times.push_back(std::stof(item));

    std::cout << "Capture mode: " << times.size() << " frame(s) -> " << directory << std::endl;
}


bool frame_capture_structure::finished() const
{
    return active && next >= static_cast<int>(times.size());
}


void frame_capture_structure::update(float animation_time, std::string const& model_name)
{
    if (!active || finished() || (video_fps <= 0 && animation_time < times[next]))
        return;

    // Read the current framebuffer (viewport size handles high-DPI screens)
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    int const w = viewport[2];
    int const h = viewport[3];

    image_structure im;
    im.width = w;
    im.height = h;
    im.color_type = image_color_type::rgba;
    im.data.resize(4 * w * h);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, im.data.data.data());

    std::stringstream filename;
    if (video_fps > 0)
        filename << directory << model_name << "_" << std::setw(4) << std::setfill('0') << next << ".png";
    else
        filename << directory << model_name << "_" << std::setw(2) << std::setfill('0') << next
                 << "_t" << std::fixed << std::setprecision(2) << animation_time << ".png";
    image_save_png(filename.str(), im.mirror_vertical());
    std::cout << "Saved " << filename.str() << std::endl;

    ++next;
    if (video_fps > 0)
        virtual_time += 1.0 / video_fps;
}
