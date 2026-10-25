#pragma once

namespace cgp_test
{
	// Note: vec2/vec3/vec4 are only aliases of numarray_stack<float,N>, and dot/norm/normalize
	//  are generic numarray_stack functions. Their behaviour is therefore covered by the
	//  numarray_stack tests. This file only tests what is genuinely specific to 05_vec:
	//  the cross product (defined for vec3) and a small smoke-test that the vec aliases
	//  resolve to the expected numarray functions.
	void test_vec();
}
