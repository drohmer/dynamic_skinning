#pragma once

#include "animated_character/animated_character.hpp"

// Normalization applied to the display coordinates: p' = scaling * (p - center)
struct normalization_transform {
    float scaling = 1.0f;
    cgp::vec3 center;
};

// Normalize the character so that its deformed mesh fits within [-1, 1]
// on the largest axis. Call after loading, before use.
// Return the transform, to place other objects of the scene (e.g. the track of the train).
normalization_transform normalize_character(character_structure& character);
