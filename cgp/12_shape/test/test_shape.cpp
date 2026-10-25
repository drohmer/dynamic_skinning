#include "test_shape.hpp"

#include "cgp/01_base/base.hpp"
#include "cgp/12_shape/intersection/intersection.hpp"
#include "cgp/12_shape/bounding_box/bounding_box.hpp"
#include "cgp/12_shape/spatial_domain/spatial_domain_grid_3D/spatial_domain_grid_3D.hpp"
#include "cgp/12_shape/implicit/marching_cube/marching_cube.hpp"

#include <cmath>

using namespace cgp;

namespace cgp_test
{
	void test_shape()
	{
		// ---------- ray / sphere intersection ----------
		{
			// Ray from origin along +x, sphere centered at {5,0,0} radius 1: hit at {4,0,0}.
			intersection_structure const inter = intersection_ray_sphere({ 0,0,0 }, { 1,0,0 }, { 5,0,0 }, 1.0f);
			assert_cgp_no_msg(inter.valid);
			assert_cgp_no_msg(is_equal(inter.position, vec3{ 4,0,0 }));
			assert_cgp_no_msg(is_equal(inter.normal, vec3{ -1,0,0 }));

			// Ray pointing away from the sphere: no intersection.
			intersection_structure const miss = intersection_ray_sphere({ 0,0,0 }, { -1,0,0 }, { 5,0,0 }, 1.0f);
			assert_cgp_no_msg(miss.valid == false);
		}

		// ---------- ray / plane intersection ----------
		{
			// Ray along +z hitting the plane z=5.
			intersection_structure const hit = intersection_ray_plane({ 0,0,0 }, { 0,0,1 }, { 0,0,5 }, { 0,0,1 });
			assert_cgp_no_msg(hit.valid);
			assert_cgp_no_msg(is_equal(hit.position, vec3{ 0,0,5 }));

			// Ray parallel to the plane (direction orthogonal to the normal): no intersection,
			//  must not divide by zero nor report a spurious hit at infinity.
			intersection_structure const parallel = intersection_ray_plane({ 0,0,0 }, { 1,0,0 }, { 0,0,5 }, { 0,0,1 });
			assert_cgp_no_msg(parallel.valid == false);
		}

		// ---------- bounding_box ----------
		{
			numarray<vec3> const pts = { {-1,-2,-3}, {1,2,3}, {0,0,0} };
			bounding_box bb;
			bb.initialize(pts);
			assert_cgp_no_msg(is_equal(bb.p_min, vec3{ -1,-2,-3 }));
			assert_cgp_no_msg(is_equal(bb.p_max, vec3{ 1,2,3 }));
			assert_cgp_no_msg(bb.inside({ 0,0,0 }));
			assert_cgp_no_msg(bb.inside({ 2,0,0 }) == false);

			bounding_box bb2;
			bb2.initialize(numarray<vec3>{ {0.5f, 0.5f, 0.5f}, { 5,5,5 } });
			assert_cgp_no_msg(bounding_box::collide(bb, bb2));      // overlap around {0.5,...}
			bounding_box bb3;
			bb3.initialize(numarray<vec3>{ {10,10,10}, { 11,11,11 } });
			assert_cgp_no_msg(bounding_box::collide(bb, bb3) == false);
		}

		// ---------- spatial_domain_grid_3D position mapping ----------
		{
			spatial_domain_grid_3D const d = spatial_domain_grid_3D::from_center_length({ 0,0,0 }, { 2,2,2 }, { 3,3,3 });
			assert_cgp_no_msg(is_equal(d.corner_min(), vec3{ -1,-1,-1 }));
			assert_cgp_no_msg(is_equal(d.corner_max(), vec3{ 1,1,1 }));
			assert_cgp_no_msg(is_equal(d.position({ 0,0,0 }), vec3{ -1,-1,-1 }));
			assert_cgp_no_msg(is_equal(d.position({ 2,2,2 }), vec3{ 1,1,1 }));
			assert_cgp_no_msg(is_equal(d.position({ 1,1,1 }), vec3{ 0,0,0 }));
		}

		// ---------- marching_cube welds shared-edge vertices across voxels ----------
		// Sphere SDF on a regular grid. With a generic radius (surface never crossing a grid
		//  vertex), every output vertex lies strictly inside a single grid edge, so distinct
		//  vertices must have distinct positions. A duplicate would mean a shared edge was not
		//  welded (the bug: an order-sensitive dedup key).
		{
			int const N = 11;
			float const radius = 0.66f;
			spatial_domain_grid_3D const domain =
				spatial_domain_grid_3D::from_center_length({ 0,0,0 }, { 2,2,2 }, { N,N,N });

			grid_3D<float> field(int3{ N,N,N });
			for (int kz = 0; kz < N; ++kz)
				for (int ky = 0; ky < N; ++ky)
					for (int kx = 0; kx < N; ++kx) {
						vec3 const p = domain.position({ kx,ky,kz });
						field(kx, ky, kz) = norm(p) - radius; // <0 inside, >0 outside
					}

			mesh const m = marching_cube(field, domain, 0.0f);
			assert_cgp_no_msg(m.position.size() > 0);
			assert_cgp_no_msg(m.connectivity.size() > 0);

			// No two welded vertices may share the same position.
			int const V = m.position.size();
			for (int i = 0; i < V; ++i)
				for (int j = i + 1; j < V; ++j)
					assert_cgp_no_msg(is_equal(m.position[i], m.position[j]) == false);
		}
	}
}
