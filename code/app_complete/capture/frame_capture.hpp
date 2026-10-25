#pragma once

#include "cgp/cgp.hpp"

#include <string>
#include <vector>


// Automatic screenshot mode, driven by environment variables:
//   DS_CAPTURE_DIR    output directory (enables the capture mode)
//   DS_CAPTURE_MODEL  model to load (default: the scene default)
//   DS_CAPTURE_TIMES  comma-separated animation times (s) at which a frame is saved,
//                     e.g. "0,0.5,1.0" (default: "0")
//   DS_CAPTURE_DS     0 to disable dynamic skinning (default: unchanged)
//   DS_CAPTURE_DELAY  0/1 to disable/enable the delayed LBS (default: model default)
//   DS_CAPTURE_FPS    frame rate imposed without vsync, e.g. 30 or 120 (default: unchanged)
//   DS_CAPTURE_GPU    0/1 to force the CPU/GPU skinning path (default: model default)
//   DS_CAPTURE_EYE    "x,y,z" camera position, looking at the preset center with its up vector (default: model preset)
//   DS_CAPTURE_GUI    1 to include the GUI in the frames, 2 to also open the settings window (default: 0)
//   DS_CAPTURE_SKELETON 0/1 to hide/show the skeleton (default: unchanged)
// The animation is started, frames are saved as <dir>/<model>_<k>_t<time>.png (without GUI by default),
// and the application quits after the last frame.
struct frame_capture_structure {
    bool active = false;
    std::string directory;
    std::string model;
    std::vector<float> times;
    int dynamic_skinning = -1;   // -1: unchanged, 0: off, 1: on
    int delay = -1;              // -1: unchanged, 0: off, 1: on
    float fps = 0;               // 0: unchanged
    int gpu = -1;                // -1: unchanged, 0: CPU path, 1: GPU path
    int skeleton = -1;           // -1: unchanged, 0: hidden, 1: displayed
    bool has_eye = false;
    cgp::vec3 eye;
    int gui = 0;                 // 0: no GUI, 1: with GUI, 2: with GUI and settings window
    int next = 0;

    // Read the environment variables
    void initialize_from_environment();

    // True when all the frames have been saved
    bool finished() const;

    // Save the current framebuffer if the animation time reached the next capture time
    void update(float animation_time, std::string const& model_name);
};
