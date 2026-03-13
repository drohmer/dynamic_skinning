#pragma once

#include "animated_character/animated_character.hpp"

// Normalize the character so that its deformed mesh fits within [-1, 1]
// on the largest axis. Call after loading, before use.
void normalize_character(character_structure& character);
