#pragma once

namespace cgp_test
{
	// Tests on the shape module (module 12, GL-free):
	//  - ray/sphere and ray/plane intersection (including the parallel-ray guard)
	//  - bounding_box initialize / inside / collide
	//  - spatial_domain_grid_3D position mapping
	//  - marching_cube welds vertices shared across neighbouring voxels
	void test_shape();
}
