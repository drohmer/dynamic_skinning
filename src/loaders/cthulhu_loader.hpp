#pragma once

#include "cgp/cgp.hpp"
#include "animated_character/animated_character.hpp"

#include <string>

// Available cthulhu resolutions
// "default" uses the root binary/ and rig/ directories (~250K vertices)
// "300000", "500000", "1500000" use the corresponding subdirectories
inline const char* cthulhu_resolutions[] = {"default", "300000", "500000", "1500000"};
inline constexpr int cthulhu_resolution_count = 4;

// Load cthulhu character from binary asset format
// base_dir should end with '/', e.g. project::path + "assets/cthulhu/"
// resolution: "default", "300000", "500000", or "1500000"
// use_bend_weights: if true, load per-vertex γ^bend weights from floppy_weights file
character_structure load_cthulhu_character(std::string const& base_dir,
                                          std::string const& resolution = "default",
                                          bool use_bend_weights = true);
