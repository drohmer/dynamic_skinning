#include "test_geometric_transformation.hpp"

#include "cgp/01_base/base.hpp"
#include "cgp/09_geometric_transformation/geometric_transformation.hpp"

#include <cmath>

#if defined(__linux__) || defined(__EMSCRIPTEN__)
#pragma GCC diagnostic ignored "-Wunused-variable"
#endif

using namespace cgp;

namespace cgp_test
{
	void test_geometric_transformation()
	{
		// frame::ux/uy/uz must return the COLUMNS of the orientation matrix
		//  (the local axes expressed in world coordinates = R * world_basis), not the rows.
		{
			rotation_transform const R = rotation_transform::from_axis_angle(vec3{0,0,1}, Pi/2.0f);
			frame const f(R, vec3{0,0,0});

			// R is a +90 deg rotation around z: x->y, y->-x, z->z
			assert_cgp_no_msg(is_equal(f.ux(), vec3{0,1,0}));
			assert_cgp_no_msg(is_equal(f.uy(), vec3{-1,0,0}));
			assert_cgp_no_msg(is_equal(f.uz(), vec3{0,0,1}));

			// must match R applied to the world basis vectors
			assert_cgp_no_msg(is_equal(f.ux(), R*vec3{1,0,0}));
			assert_cgp_no_msg(is_equal(f.uy(), R*vec3{0,1,0}));
			assert_cgp_no_msg(is_equal(f.uz(), R*vec3{0,0,1}));
		}

		// interpolation_bilinear: a function linear in x and y is interpolated exactly,
		//  including up to the last valid coordinate (dimension-2).
		{
			grid_2D<float> g;
			g.resize(3, 3);
			for (int kx = 0; kx < 3; ++kx)
				for (int ky = 0; ky < 3; ++ky)
					g(kx, ky) = float(kx) + 2.0f*float(ky); // f(x,y) = x + 2y

			assert_cgp_no_msg(is_equal(interpolation_bilinear(g, 1.5f, 0.5f), 1.5f + 2.0f*0.5f));
			assert_cgp_no_msg(is_equal(interpolation_bilinear(g, 0.0f, 0.0f), 0.0f));
			// upper bound: x in [0, dim.x-2] = [0,1], here x close to 1 with x1 = 2 still valid
			assert_cgp_no_msg(is_equal(interpolation_bilinear(g, 1.9f, 1.9f), 1.9f + 2.0f*1.9f));
		}

		// projection matrices: forward * inverse == identity
		{
			mat4 const I = mat4::build_identity();
			{
				mat4 const P    = projection_perspective(1.0f, 1.5f, 1.0f, 10.0f);
				mat4 const Pinv = projection_perspective_inverse(1.0f, 1.5f, 1.0f, 10.0f);
				assert_cgp_no_msg(is_equal(P*Pinv, I));
				assert_cgp_no_msg(is_equal(Pinv*P, I));
			}
			{
				mat4 const P    = projection_orthographic(-2.0f, 3.0f, -1.0f, 4.0f, 1.0f, 10.0f);
				mat4 const Pinv = projection_orthographic_inverse(-2.0f, 3.0f, -1.0f, 4.0f, 1.0f, 10.0f);
				assert_cgp_no_msg(is_equal(P*Pinv, I));
				assert_cgp_no_msg(is_equal(Pinv*P, I));
			}
		}

		// affine_rts inverse: inverse(T) must undo T (translation divided by scaling).
		{
			affine_rts const T(rotation_transform::from_axis_angle(vec3{0,0,1}, 0.7f), vec3{1,2,3}, 2.0f);
			affine_rts const Ti = inverse(T);

			vec3 const p{4,-5,6};
			assert_cgp_no_msg(is_equal(Ti*(T*p), p));
			assert_cgp_no_msg(is_equal(T*(Ti*p), p));
			assert_cgp_no_msg(type_str(Ti) == "affine_rts");
		}
	}
}
