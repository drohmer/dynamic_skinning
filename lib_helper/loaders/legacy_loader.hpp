#pragma once

#include "cgp/cgp.hpp"
#include "animated_character/animated_character.hpp"

#include <string>
#include <vector>

struct legacy_model_entry {
    std::string name;
    std::string directory;     // relative to the root of the project, e.g. "assets/dragon/"
    float scaling = 1.0f;
    bool invert_normals = false;
    cgp::rotation_transform rotation;  // applied to the whole model (e.g. Z-up -> Y-up)
};

// Load a character from the old code_a_modifier asset format (directory: full path, ending with '/')
// (skeleton_joint_name, skeleton_rest_pose_translation/rotation, rig_joint, rig_weights, mesh.obj, skeleton_animation)
character_structure load_legacy_character(std::string const& directory, float scaling = 1.0f, bool invert_normals = false,
                                          cgp::rotation_transform const& rotation = cgp::rotation_transform());

// List of all available legacy models
std::vector<legacy_model_entry> get_legacy_models();
