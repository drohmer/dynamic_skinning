#pragma once

namespace cgp_test
{
	void test_matrix_stack();

	// Compile-time conformity check: ensures mat2/mat3/mat4 (which are full specializations
	//  of matrix_stack and therefore do NOT inherit anything) expose the same member/static
	//  API as the generic matrix_stack. A method forgotten on a specialization (as happened
	//  with coeff / build_zero) then becomes a compile error instead of a silent gap.
	void test_matrix_conformity();
}

