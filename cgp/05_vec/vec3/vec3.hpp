#pragma once

#include "cgp/02_numarray/numarray_stack/numarray_stack.hpp"

#include <iostream>


namespace cgp
{

	// vec3 is an alias on numarray_stack<float, 3>.
	//  The alias itself, as well as dot() and norm(), are provided generically for every
	//  numarray_stack in cgp/02_numarray (see special_types.hpp and numarray_stack.hpp).
	//  The only function specific to vec3 is the cross product, declared below.

	// A vec3 can be used as
	//   struct vec3 { float x, y, z; }
	//   (with additional functions handled as a numarray_stack)

	inline vec3 cross(vec3 const& a, vec3 const& b);
}

namespace cgp
{
	inline vec3 cross(vec3 const& a, vec3 const& b)
	{
		return vec3(
			a.y * b.z - a.z * b.y,
			a.z * b.x - a.x * b.z,
			a.x * b.y - a.y * b.x
		);
	}
}
