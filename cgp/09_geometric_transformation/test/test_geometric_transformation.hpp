#pragma once

namespace cgp_test
{
	// Regression tests for bugs previously found in 09_geometric_transformation:
	//  - frame::ux/uy/uz returned matrix rows instead of columns
	//  - interpolation_bilinear valid range
	//  - projection inverse matrices (perspective and orthographic)
	//  - affine_rts inverse (translation must be divided by the scaling)
	void test_geometric_transformation();
}
