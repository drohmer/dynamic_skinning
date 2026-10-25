#pragma once

namespace cgp_test
{
	// Tests on the mesh module (module 11, GL-free):
	//  - normal_per_vertex zeroes the output buffer before accumulation (buffer reuse)
	//  - mesh_primitive_ellipsoid normals match the analytic ellipsoid gradient
	//  - mesh::apply_transform(affine) transforms normals by the inverse-transpose
	//  - closed cylinder / cone caps have outward-facing normals
	void test_mesh();
}
