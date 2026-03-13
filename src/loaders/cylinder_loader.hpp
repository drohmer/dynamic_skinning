#pragma once

#include "cgp/cgp.hpp"
#include "animated_character/animated_model/animated_model.hpp"
#include "animated_character/animated_character.hpp"


// Load a procedural bending cylinder with 3 joints and a simple bending animation.
// Returns a fully initialized character_structure ready for display and skinning.
character_structure load_cylinder_bending();
