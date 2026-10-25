#include "test_vec_mat.hpp"

#include "cgp/01_base/base.hpp"
#include "../mat_functions.hpp"

namespace cgp_test
{
	void test_vec_mat()
	{
		using namespace cgp;

		//det and inverse mat2
		{
			mat2 a{1.0f, 1.5f, 2.5f, 3.1f};
			assert_cgp_no_msg( is_equal(det(a),-0.65f) );
			assert_cgp_no_msg( is_equal(inverse(a),mat2{-4.769230f, 2.307692f, 3.846153f, -1.538461f}) );
			assert_cgp_no_msg( is_equal(inverse(a)*a, mat2::build_identity()) );
		}

		//det and inverse mat3
		{
			mat3 a{1.0f, 1.5f,2.5f, 3.1f,-1.5f,2.2f, 3.1f,1.4f,-2.4f};
			assert_cgp_no_msg( is_equal(det(a),44.385f) );
			assert_cgp_no_msg( is_equal(inverse(a),mat3{0.011716f,0.159964f,0.158837f, 0.321280f,-0.228681f,0.125042f, 0.202546f,0.073223f,-0.138560f}) );
			assert_cgp_no_msg( is_equal(inverse(a)*a, mat3::build_identity()) );
		}

		//det and inverse mat4
		{
			mat4 a{1.0f,1.5f,2.5f,-2.4f, 3.1f,-1.5f,2.2f,4.0f, 3.1f,1.4f,-2.4f,-3.5f, 5.1f,0.2f,0.5f,-0.4f};

			assert_cgp_no_msg( is_equal(det(a),-18.011389f) );
			assert_cgp_no_msg( is_equal(inverse(a),mat4{-0.019266f, 0.049025f, 0.053522f, 0.137524f, 1.500107f, 3.992530f, 4.132275f, -5.232745f, 0.040585f, -0.506735f, -0.689119f, 0.718934f, 0.555149f, 1.987909f, 1.887139f, -2.464274f}) );
			assert_cgp_no_msg( norm(inverse(a)*a - mat4::build_identity())<1e-2f );
		}


		// orthogonal vector vec2
		{
			assert_cgp_no_msg( is_equal(dot(orthogonal_vector(vec2{1,0}),vec2{1,0}),0.0f) );
			assert_cgp_no_msg( is_equal(norm(orthogonal_vector(vec2{1,0})),1.0f) );

			assert_cgp_no_msg( is_equal(dot(orthogonal_vector(vec2{0,1}),vec2{0,1}),0.0f) );
			assert_cgp_no_msg( is_equal(norm(orthogonal_vector(vec2{0,1})),1.0f) );

			assert_cgp_no_msg( is_equal(dot(orthogonal_vector(vec2{5,3}),vec2{5,3}),0.0f) );
			assert_cgp_no_msg( is_equal(norm(orthogonal_vector(vec2{5,3})),1.0f) );
		}

		// orthogonal vector vec3
		{
			assert_cgp_no_msg( is_equal(dot(orthogonal_vector(vec3{1,0,0}),vec3{1,0,0}),0.0f) );
			assert_cgp_no_msg( is_equal(norm(orthogonal_vector(vec3{1,0,0})),1.0f) );

			assert_cgp_no_msg( is_equal(dot(orthogonal_vector(vec3{0,1,0}),vec3{0,1,0}),0.0f) );
			assert_cgp_no_msg( is_equal(norm(orthogonal_vector(vec3{0,1,0})),1.0f) );

			assert_cgp_no_msg( is_equal(dot(orthogonal_vector(vec3{0,0,1}),vec3{0,0,1}),0.0f) );
			assert_cgp_no_msg( is_equal(norm(orthogonal_vector(vec3{0,0,1})),1.0f) );

			assert_cgp_no_msg( is_equal(dot(orthogonal_vector(vec3{5,3,-4}),vec3{5,3,-4}),0.0f) );
			assert_cgp_no_msg( is_equal(norm(orthogonal_vector(vec3{5,3,-4})),1.0f) );
		}


	// build_diagonal(vec) — last diagonal element must be on the diagonal, not off
	{
		assert_cgp_no_msg( is_equal(mat3::build_diagonal(vec3{2,3,5}), mat3{2,0,0, 0,3,0, 0,0,5}) );
		assert_cgp_no_msg( is_equal(mat2::build_diagonal(vec2{4,7}),   mat2{4,0, 0,7}) );
		assert_cgp_no_msg( is_equal(mat4::build_diagonal(vec4{1,2,3,4}), mat4{1,0,0,0, 0,2,0,0, 0,0,3,0, 0,0,0,4}) );
	}

	// tensor_product — every row i must use a[i], not a mix of components
	{
		vec4 a = {1,2,3,4};
		vec4 b = {5,6,7,8};
		mat4 expected = {1*5,1*6,1*7,1*8, 2*5,2*6,2*7,2*8, 3*5,3*6,3*7,3*8, 4*5,4*6,4*7,4*8};
		assert_cgp_no_msg( is_equal(tensor_product(a,b), expected) );
	}

	}
}