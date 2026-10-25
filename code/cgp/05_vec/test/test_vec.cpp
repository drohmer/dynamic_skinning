#include "test_vec.hpp"

#include "cgp/01_base/base.hpp"
#include "cgp/05_vec/vec.hpp"

#include <cmath>

#if defined(__linux__) || defined(__EMSCRIPTEN__)
#pragma GCC diagnostic ignored "-Wunused-variable"
#endif

using namespace cgp;

namespace cgp_test
{
	void test_vec()
	{
		// cross product (the only function specific to 05_vec)
		{
			vec3 const x{1,0,0};
			vec3 const y{0,1,0};
			vec3 const z{0,0,1};

			assert_cgp_no_msg(is_equal(cross(x,y), z));
			assert_cgp_no_msg(is_equal(cross(y,z), x));
			assert_cgp_no_msg(is_equal(cross(z,x), y));

			// anti-commutativity and self-cross
			assert_cgp_no_msg(is_equal(cross(y,x), -z));
			assert_cgp_no_msg(is_equal(cross(x,x), vec3{0,0,0}));

			// cross(a,b) is orthogonal to both a and b
			vec3 const a{1,2,3};
			vec3 const b{-2,0,4};
			vec3 const c = cross(a,b);
			assert_cgp_no_msg(is_equal(dot(c,a), 0.0f));
			assert_cgp_no_msg(is_equal(dot(c,b), 0.0f));
		}

		// smoke-test: the vec aliases resolve to the generic numarray dot/norm/normalize
		{
			vec3 const v{3,4,0};
			assert_cgp_no_msg(is_equal(norm(v), 5.0f));
			assert_cgp_no_msg(is_equal(dot(v,v), 25.0f));
			assert_cgp_no_msg(is_equal(norm(normalize(v)), 1.0f));

			// named accessors of the vec specializations
			vec4 const w{1,2,3,4};
			assert_cgp_no_msg(is_equal(w.x,1.0f) && is_equal(w.y,2.0f) && is_equal(w.z,3.0f) && is_equal(w.w,4.0f));
			assert_cgp_no_msg(is_equal(w.xyz(), vec3{1,2,3}));
		}

		// scalar scaling keeps the scalar type (no longer frozen to float): a double-valued
		//  numarray scaled by a double scalar must keep full double precision. 0.1 is not
		//  representable in float, so float-freezing would change the result.
		{
			numarray_stack<double,2> const a{1.0, 1.0};
			numarray_stack<double,2> const r = a * 0.1;     // 0.1 as a double scalar
			assert_cgp_no_msg(r[0] == 0.1 && r[1] == 0.1);  // exact: would fail if scaled through float
			// float case stays exactly as before
			assert_cgp_no_msg(is_equal(vec3{1,2,3} * 2.0f, vec3{2,4,6}));
		}
	}
}
