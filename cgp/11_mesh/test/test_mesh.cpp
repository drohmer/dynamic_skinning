#include "test_mesh.hpp"

#include "cgp/01_base/base.hpp"
#include "cgp/11_mesh/mesh/mesh.hpp"
#include "cgp/11_mesh/primitive/primitive.hpp"

#include <cmath>

using namespace cgp;

namespace cgp_test
{
	void test_mesh()
	{
		// ---------- normal_per_vertex zeroes the buffer before accumulation ----------
		// Reuse a buffer of a different (smaller) size: the function must resize AND
		//  zero it, otherwise stale values leak into the accumulation.
		{
			numarray<vec3> normals;
			normals.resize(3);
			normals.fill(vec3{ 5, 5, 5 }); // garbage that must not survive the resize-to-6

			// Two triangles in the z=0 plane: every vertex normal must be exactly {0,0,1}
			numarray<vec3> const position = {
				{0,0,0}, {1,0,0}, {1,1,0},
				{0,0,0}, {1,1,0}, {0,1,0} };
			numarray<uint3> const connectivity = { {0,1,2}, {3,4,5} };

			normal_per_vertex(position, connectivity, normals);

			assert_cgp_no_msg(normals.size() == 6);
			for (int k = 0; k < normals.size(); ++k)
				assert_cgp_no_msg(is_equal(normals[k], vec3{ 0,0,1 }));
		}

		// ---------- ellipsoid normals match the analytic gradient ----------
		// For an ellipsoid (x/a)^2+(y/b)^2+(z/c)^2=1 centered at 0, the outward normal
		//  at point p is parallel to {p.x/a^2, p.y/b^2, p.z/c^2}. A sphere normal (the
		//  former bug) is not parallel to this for non-uniform scale.
		{
			vec3 const s = { 1.0f, 2.0f, 3.0f };
			mesh const e = mesh_primitive_ellipsoid(s, { 0,0,0 }, 20, 12);
			assert_cgp_no_msg(e.normal.size() == e.position.size());

			for (int k = 0; k < e.position.size(); ++k)
			{
				vec3 const p = e.position[k];
				vec3 const g = { p.x / (s.x*s.x), p.y / (s.y*s.y), p.z / (s.z*s.z) };
				float const Lg = norm(g);
				if (Lg > 1e-6f)
				{
					vec3 const gd = g / Lg;
					vec3 const n = e.normal[k];
					assert_cgp_no_msg(std::abs(norm(n) - 1.0f) < 1e-4f); // unit length
					assert_cgp_no_msg(norm(cross(gd, n)) < 1e-3f);       // parallel to gradient
					assert_cgp_no_msg(dot(gd, n) > 0.0f);                // outward (same direction)
				}
			}
		}

		// ---------- apply_transform(affine) transforms normals by inverse-transpose ----------
		// Non-uniform scaling_xyz={1,2,1} on a normal n0=normalize({1,1,0}).
		// Correct result: normalize( diag(1,1/2,1) * n0 ) = normalize({2,1,0}).
		{
			mesh m;
			m.position = { {0,0,0} };
			m.normal = { normalize(vec3{ 1,1,0 }) };

			affine const M(rotation_transform(), vec3{ 0,0,0 }, 1.0f, vec3{ 1,2,1 });
			m.apply_transform(M);

			assert_cgp_no_msg(is_equal(m.normal[0], normalize(vec3{ 2,1,0 })));
		}

		// ---------- closed cylinder / cone caps face outward ----------
		// Axis along +z. Side normals have z=0; the top cap faces +z; the bottom cap (resp.
		//  cone base) must face -z. The former bug left the bottom cap facing +z (inward).
		{
			mesh const cyl = mesh_primitive_cylinder(0.5f, { 0,0,0 }, { 0,0,1 }, 4, 12, true);
			bool has_down = false, has_up = false;
			for (int k = 0; k < cyl.normal.size(); ++k) {
				if (is_equal(cyl.normal[k], vec3{ 0,0,-1 })) has_down = true;
				if (is_equal(cyl.normal[k], vec3{ 0,0, 1 })) has_up = true;
			}
			assert_cgp_no_msg(has_down); // bottom cap outward (-z)
			assert_cgp_no_msg(has_up);   // top cap outward (+z)
		}
		{
			mesh const cone = mesh_primitive_cone(0.5f, 1.0f, { 0,0,0 }, { 0,0,1 }, true, 16, 8);
			bool has_down = false;
			for (int k = 0; k < cone.normal.size(); ++k)
				if (is_equal(cone.normal[k], vec3{ 0,0,-1 })) has_down = true;
			assert_cgp_no_msg(has_down); // base outward (-z)
		}

		// ---------- negative uniform scale mirrors geometry (inverse-transpose: negate) ----------
		// A triangle in z=0 with normal +z. scale(-1) is a point inversion (det<0): the stored
		//  normal must flip to -z and the winding must be reversed to stay coherent.
		{
			mesh m;
			m.position = { {0,0,0}, {1,0,0}, {0,1,0} };
			m.normal = { {0,0,1}, {0,0,1}, {0,0,1} };
			m.connectivity = { {0,1,2} };

			m.scale(-1.0f);

			for (int k = 0; k < m.normal.size(); ++k)
				assert_cgp_no_msg(is_equal(m.normal[k], vec3{ 0,0,-1 }));
			uint3 const& c = m.connectivity[0];
			assert_cgp_no_msg(c[0] == 1u && c[1] == 0u && c[2] == 2u); // winding reversed
		}

		// ---------- single-axis negative scale is a reflection (normal stays +z) ----------
		// Under diag(-1,1,1) the inverse-transpose keeps a +z normal as +z. The winding flip
		//  + normal recomputation in scale(sx,sy,sz) must yield exactly that.
		{
			mesh m;
			m.position = { {0,0,0}, {1,0,0}, {0,1,0} };
			m.normal = { {0,0,1}, {0,0,1}, {0,0,1} };
			m.connectivity = { {0,1,2} };

			m.scale(-1.0f, 1.0f, 1.0f);

			for (int k = 0; k < m.normal.size(); ++k)
				assert_cgp_no_msg(is_equal(m.normal[k], vec3{ 0,0,1 }));
		}
	}
}
