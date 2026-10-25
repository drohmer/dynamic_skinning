#include "cgp/05_vec/vec.hpp"
#include "cgp/06_mat/mat.hpp"

#if defined(__linux__) || defined(__EMSCRIPTEN__)
#pragma GCC diagnostic ignored "-Wunused-variable"
#endif

#include <iostream>

namespace cgp_test 
{

	void test_matrix_stack()
	{
		// element access
		{
			using namespace cgp;
			matrix_stack<int, 2, 3> a = { 1,2,3, 4,5,6 };
			
			assert_cgp_no_msg(a(0, 0) == 1); assert_cgp_no_msg(a(0, 1) == 2); assert_cgp_no_msg(a(0, 2) == 3);
			assert_cgp_no_msg(a(1, 0) == 4); assert_cgp_no_msg(a(1, 1) == 5); assert_cgp_no_msg(a(1, 2) == 6);

			assert_cgp_no_msg(a(0)(0) == 1); assert_cgp_no_msg(a(0)(1) == 2); assert_cgp_no_msg(a(0)(2) == 3);
			assert_cgp_no_msg(a(1)(0) == 4); assert_cgp_no_msg(a(1)(1) == 5); assert_cgp_no_msg(a(1)(2) == 6);

			assert_cgp_no_msg(a[0][0] == 1); assert_cgp_no_msg(a[0][1] == 2); assert_cgp_no_msg(a[0][2] == 3);
			assert_cgp_no_msg(a[1][0] == 4); assert_cgp_no_msg(a[1][1] == 5); assert_cgp_no_msg(a[1][2] == 6);


			assert_cgp_no_msg( is_equal(a[0], numarray_stack<int, 3>{ 1,2,3 }) && is_equal(a[1], numarray_stack<int, 3>{ 4,5,6 }) );
			assert_cgp_no_msg( is_equal(a(0), numarray_stack<int, 3>{ 1, 2, 3 }) && is_equal(a(1), numarray_stack<int, 3>{ 4, 5, 6 }));


			assert_cgp_no_msg( is_equal(get<0, 0>(a), 1) ); assert_cgp_no_msg(is_equal(get<0, 1>(a), 2)); assert_cgp_no_msg(is_equal(get<0, 2>(a), 3));
			assert_cgp_no_msg( is_equal(get<1, 0>(a), 4)); assert_cgp_no_msg(is_equal(get<1, 1>(a), 5)); assert_cgp_no_msg(is_equal(get<1, 2>(a), 6));

			assert_cgp_no_msg(is_equal(get_offset<0>(a), 1)); assert_cgp_no_msg(is_equal(get_offset<1>(a), 2)); assert_cgp_no_msg(is_equal(get_offset<2>(a), 3));
			assert_cgp_no_msg(is_equal(get_offset<3>(a), 4)); assert_cgp_no_msg(is_equal(get_offset<4>(a), 5)); assert_cgp_no_msg(is_equal(get_offset<5>(a), 6));

			assert_cgp_no_msg(is_equal(get<0>(a), numarray_stack<int, 3>{ 1,2,3 }) && is_equal(get<1>(a), numarray_stack<int, 3>{ 4,5,6 }) );

			assert_cgp_no_msg(type_str(a) == "matrix_stack<int,2,3>");
		}

		// matrix multiplication
		{
			cgp::matrix_stack<int, 2, 3> a = { 1,2,3, 4,5,6 };
			cgp::matrix_stack<int, 3, 2> b = { 5,-1, 4,8, -2,2 };

			cgp::matrix_stack<int, 2, 2> c = a * b;
			assert_cgp_no_msg(is_equal(c, cgp::matrix_stack<int, 2,2>{7,21,28,48}));
		}


		// matrix componentwise multiplication
		{
			cgp::matrix_stack<int, 2, 3> a = { 1,2,3, 4,5,6 };
			cgp::matrix_stack<int, 2, 3> b = { 5,2,-1, 2,1,-2 };

			cgp::matrix_stack<int, 2, 3> c = cgp::multiply_componentwise(a, b);
			assert_cgp_no_msg(is_equal(c, cgp::matrix_stack<int, 2, 3>{5,4,-3, 8,5,-12}));
		}

		// matrix vector
		{
			cgp::matrix_stack<int, 2, 3> a = { 1,2,3, 4,5,6 };
			cgp::numarray_stack<int, 3> b = { 5,-2,3 };
			cgp::numarray_stack<int, 2> c = a * b;
			assert_cgp_no_msg(is_equal(c, cgp::numarray_stack<int, 2>{10, 28}));
		}


		// remove column row
		{
			cgp::matrix_stack<int, 5, 4> a = { 1,2,3,4, 5,6,7,8, 9,10,11,12, 13,14,15,16, 17,18,19,20};

			assert_cgp_no_msg( is_equal(a.remove_row_column(1, 2), cgp::matrix_stack<int,4,3>{1,2,4, 9,10,12, 13,14,16, 17,18,20} ) );
			assert_cgp_no_msg( is_equal(a.remove_row_column(0, 0), cgp::matrix_stack<int,4,3>{6,7,8, 10,11,12, 14,15,16, 18,19,20} ) );
			assert_cgp_no_msg( is_equal(a.remove_row_column(4, 3), cgp::matrix_stack<int,4,3>{1,2,3, 5,6,7, 9,10,11, 13,14,15} ) );


		}

		// identity
		{
			assert_cgp_no_msg(is_equal(cgp::matrix_stack<float, 3, 3>::build_identity(), cgp::matrix_stack<float, 3, 3>{ 1,0,0, 0,1,0, 0,0,1 }));
			assert_cgp_no_msg(is_equal(cgp::matrix_stack<float, 3, 4>::build_identity(), cgp::matrix_stack<float, 3, 4>{ 1,0,0,0, 0,1,0,0, 0,0,1,0 }));
			assert_cgp_no_msg(is_equal(cgp::matrix_stack<float, 4, 3>::build_identity(), cgp::matrix_stack<float, 4, 3>{ 1,0,0, 0,1,0, 0,0,1, 0,0,0 }));
		}

		// construct from different size
		{
			using namespace cgp;
			{
				mat3 M1 = { 1,2,3,
							4,5,6,
							7,8,9 };
				mat2 M2 = mat2(M1);
				mat4 M4 = mat4(M1);
				assert_cgp_no_msg(is_equal(M2, mat2{ 1,2,4,5 }));
				assert_cgp_no_msg(is_equal(M4, mat4{ 1,2,3,0, 4,5,6,0, 7,8,9,0, 0,0,0,1 }));
			}
			{
				mat4 M1 = { 1 , 2, 3, 4,
							5 , 6, 7, 8,
							9 ,10,11,12,
							13,14,15,16
				};
				mat2 M2 = mat2(M1);
				mat3 M3 = mat3(M1);
				assert_cgp_no_msg(is_equal(M2, mat2{ 1,2,5,6 }));
				assert_cgp_no_msg(is_equal(M3, mat3{ 1,2,3, 5,6,7, 9,10,11 }));
			}
		}

		// set block
		{
			using namespace cgp;

			assert_cgp_no_msg( is_equal(mat4().set_block(mat2{ 1,2,3,4 }), mat4{ 1,2,0,0, 3,4,0,0, 0,0,0,0, 0,0,0,0 }));
			assert_cgp_no_msg( is_equal(mat4().set_block(mat2{ 1,2,3,4 }, 0, 1), mat4{ 0,1,2,0, 0,3,4,0, 0,0,0,0, 0,0,0,0 }));
			assert_cgp_no_msg( is_equal(mat4().set_block(mat2{ 1,2,3,4 }, 1, 0), mat4{ 0,0,0,0, 1,2,0,0, 3,4,0,0, 0,0,0,0 }));
		}


		// mat * vec
		{
			using namespace cgp;

			{
				mat2 const M = { 1,2, 3,4 };
				vec2 const x = { 4,2 };
				assert_cgp_no_msg(is_equal(M * x, vec2{ 8,20 }));
			}

			{
				mat3 const M = { 1,2,1, 3,4,5, 1,2,7 };
				vec3 const x = { 4,2,6 };
				assert_cgp_no_msg(is_equal(M * x, vec3{ 14,50,50 }));
			}

			{
				mat4 const M = { 1,2,1,5, 3,4,5,-2, 1,2,7,3, 5,4,7,-1 };
				vec4 const x = { 4,2,6,3 };
				assert_cgp_no_msg(is_equal(M * x, vec4{ 29,44,59,67 }));
			}
		}

		// Operators
		{
			using namespace cgp;
			{
				cgp::matrix_stack<int, 3, 2> A = {2,4, 5,1, 0,1};
				cgp::matrix_stack<int, 3, 2> B = {4,1, 1,2, 3,2};

				A -= B;
				assert_cgp_no_msg(is_equal(A, cgp::matrix_stack<int, 3, 2>{-2,3, 4,-1, -3,-1}));
				A += B;
				assert_cgp_no_msg(is_equal(A, cgp::matrix_stack<int, 3, 2>{2, 4, 5, 1, 0, 1}));
			}

			{
				cgp::matrix_stack<int, 2, 3> A = { 2,4,5, 1,0,1 };
				int b=2;
				A *= b;
				assert_cgp_no_msg(is_equal(A, cgp::matrix_stack<int, 2, 3>{4,8,10, 2,0,2}));
			}

			{
				cgp::matrix_stack<float, 2, 3> A = { 2,4,5, 1,0,1 };
				float b = 2;
				A /= b;
				assert_cgp_no_msg(is_equal(A, cgp::matrix_stack<float, 2, 3>{1,2, 2.5,0.5, 0,0.5}));
			}

		}


		{
			using namespace cgp;
			{
				mat2 a = { 1,2, 3,4 };
				a *= 2.0f;
				assert_cgp_no_msg(is_equal(a, mat2{ 2,4, 6,8 }));
			}
			{
				mat2 const a = { 1,2, 3,4 };
				mat2 b = -a;
				assert_cgp_no_msg(is_equal(b, mat2{ -1,-2, -3,-4 }));
			}

		}

		{
			using namespace cgp;
			{
				matrix_stack<float, 3, 1> M = vec3(1, 2, 3);
				assert_cgp_no_msg(is_equal(M, matrix_stack<float,3, 1>{ 1,2,3 }));
			}
			{
				matrix_stack<float, 1, 3> M = vec3(1, 2, 3);
				assert_cgp_no_msg(is_equal(M, matrix_stack<float, 1, 3>{ 1, 2, 3 }));
			}
		}

		{
			using namespace cgp;
			{
				mat2 M2; M2.fill(2.0f);
				mat3 M3; M3.fill(2.0f);
				mat4 M4; M4.fill(2.0f);
				matrix_stack<float, 2, 3> M; M.fill(2.0f);

				assert_cgp_no_msg(is_equal(M2, mat2{ 2, 2, 2,2 }));
				assert_cgp_no_msg(is_equal(M3, mat3{ 2,2,2, 2,2,2, 2,2,2 }));
				assert_cgp_no_msg(is_equal(M4, mat4{ 2,2,2,2, 2,2,2,2, 2,2,2,2, 2,2,2,2 }));
				assert_cgp_no_msg(is_equal(M, matrix_stack<float, 2, 3>{ 2,2,2, 2,2,2 }));
			}
		}

		{
			using namespace cgp;
			{
				mat4 M = mat4::build_scaling(5, 3, 2).set_block_translation(1, 2, 3);
				vec3 t = { 4,8,-1 };
				vec3 res = M.transform_position(t);
				assert_cgp_no_msg(is_equal(res, { 21,26,1 }));
			}
			{
				mat4 M = mat4::build_scaling(5, 3, 2).set_block_translation(1, 2, 3);
				vec3 t = { 4,8,-1 };
				vec3 res = M.transform_vector(t);
				assert_cgp_no_msg(is_equal(res, { 20,24,-2 }));
			}
			{
				mat4 M = mat4::build_scaling(5, 3, 2).set_block_translation(1, 2, 3);
				vec3 t = { 4,8,-1 };
				vec4 res_tmp = M * vec4(t,1.0f);
				vec3 res = res_tmp.xyz() / res_tmp.w;
				assert_cgp_no_msg(is_equal(res, { 21,26,1 }));
			}
			{
				mat4 M = mat4::build_scaling(5, 3, 2).set_block_translation(1, 2, 3);
				vec3 t = { 4,8,-1 };
				vec3 res = (M * vec4(t,0.0)).xyz();
				assert_cgp_no_msg(is_equal(res, { 20,24,-2 }));
			}

			{
				mat4 M = mat4::build_scaling(5, 3, 2).set_block_translation(1, 2, 3);
				M(3, 3) = 2;
				vec3 t = { 4,8,-1 };
				vec3 res = M.transform_position(t);
				assert_cgp_no_msg(is_equal(res, { 21/2.0f,26/2.0f,1/2.0f }));
			}
			{
				mat4 M = mat4(
					1, 5, 8, 2, 
					2, 1, -2, 3, 
					5, 2, 8, 4, 
					0.5f, -1, 2, 2);
				vec3 t = { 4,8,-1 };
				vec3 res = M .transform_position(t);
				assert_cgp_no_msg(is_equal(res, { -38/6.0f, -21/6.0f, -32/6.0f }));
			}
		}

		// test initialization
		{
			using namespace cgp;

			mat4 M1(1.0f);
			mat4 M2(2.0f);
			mat4 M3(2.0f, 3.0f, 7.0f);
			mat4 M4(2.0f, 3.0f, 7.0f, 2.0f);

			assert_cgp_no_msg(is_equal(M1, mat4{ 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 }));
			assert_cgp_no_msg(is_equal(M2, mat4{ 2,0,0,0, 0,2,0,0, 0,0,2,0, 0,0,0,2 }));
			assert_cgp_no_msg(is_equal(M3, mat4{ 2,0,0,0, 0,3,0,0, 0,0,7,0, 0,0,0,1 }));
			assert_cgp_no_msg(is_equal(M4, mat4{ 2,0,0,0, 0,3,0,0, 0,0,7,0, 0,0,0,2 }));
		}
		// test initialization with initializer-list
		{
			using namespace cgp;

			mat4 M1 = {1.0f};
			mat4 M2 = {2.0f};
			mat4 M3 = {2.0f, 3.0f, 7.0f};
			mat4 M4 = {2.0f, 3.0f, 7.0f, 2.0f};

			assert_cgp_no_msg(is_equal(M1, mat4{ 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 }));
			assert_cgp_no_msg(is_equal(M2, mat4{ 2,0,0,0, 0,2,0,0, 0,0,2,0, 0,0,0,2 }));
			assert_cgp_no_msg(is_equal(M3, mat4{ 2,0,0,0, 0,3,0,0, 0,0,7,0, 0,0,0,1 }));
			assert_cgp_no_msg(is_equal(M4, mat4{ 2,0,0,0, 0,3,0,0, 0,0,7,0, 0,0,0,2 }));
		}
		{
			using namespace cgp;

			mat3 M1(1.0f);
			mat3 M2(2.0f);
			mat3 M3(2.0f, 3.0f, 7.0f);
			mat3 M1b = {1.0f};
			mat3 M2b = {2.0f};
			mat3 M3b = {2.0f, 3.0f, 7.0f};

			assert_cgp_no_msg(is_equal(M1, mat3{ 1,0,0, 0,1,0, 0,0,1 }));
			assert_cgp_no_msg(is_equal(M2, mat3{ 2,0,0, 0,2,0, 0,0,2 }));
			assert_cgp_no_msg(is_equal(M3, mat3{ 2,0,0, 0,3,0, 0,0,7 }));

			assert_cgp_no_msg(is_equal(M1b, mat3{ 1,0,0, 0,1,0, 0,0,1 }));
			assert_cgp_no_msg(is_equal(M2b, mat3{ 2,0,0, 0,2,0, 0,0,2 }));
			assert_cgp_no_msg(is_equal(M3b, mat3{ 2,0,0, 0,3,0, 0,0,7 }));
		}
		{
			using namespace cgp;

			mat2 M1(1.0f);
			mat2 M2(2.0f);
			mat2 M3(2.0f, 3.0f);
			mat2 M1b = {1.0f};
			mat2 M2b = {2.0f};
			mat2 M3b = {2.0f, 3.0f};

			assert_cgp_no_msg(is_equal(M1, mat2{ 1,0, 0,1 }));
			assert_cgp_no_msg(is_equal(M2, mat2{ 2,0, 0,2 }));
			assert_cgp_no_msg(is_equal(M3, mat2{ 2,0, 0,3 }));

			assert_cgp_no_msg(is_equal(M1b, mat2{ 1,0, 0,1 }));
			assert_cgp_no_msg(is_equal(M2b, mat2{ 2,0, 0,2 }));
			assert_cgp_no_msg(is_equal(M3b, mat2{ 2,0, 0,3 }));
		}



		// norm must compute sqrt(sum of squares), not sqrt(sum)
		{
			// {3,4,0,...}: sqrt(9+16) = 5
			cgp::matrix_stack<float,2,3> M = {3.0f,4.0f,0.0f, 0.0f,0.0f,0.0f};
			assert_cgp_no_msg(cgp::is_equal(cgp::norm(M), 5.0f));
			// {1,1,1,1}: sqrt(1+1+1+1) = 2
			cgp::mat2 M2 = {1.0f,1.0f, 1.0f,1.0f};
			assert_cgp_no_msg(cgp::is_equal(cgp::norm(M2), 2.0f));
			// norm on mat3 (used coeff on specialization — now uses at)
			cgp::mat3 M3 = {1.0f,0.0f,0.0f, 0.0f,1.0f,0.0f, 0.0f,0.0f,1.0f};
			assert_cgp_no_msg(cgp::is_equal(cgp::norm(M3), std::sqrt(3.0f)));
		}

		// set_block at corner offset (2,2): requires N+offset <= N_mat, not strictly <
		{
			using namespace cgp;
			assert_cgp_no_msg(is_equal(
				mat4().set_block(mat2{1,2,3,4}, 2, 2),
				mat4{0,0,0,0, 0,0,0,0, 0,0,1,2, 0,0,3,4}));
			assert_cgp_no_msg(is_equal(
				mat4().set_block(mat2{1,2,3,4}, 0, 2),
				mat4{0,0,1,2, 0,0,3,4, 0,0,0,0, 0,0,0,0}));
			assert_cgp_no_msg(is_equal(
				mat4().set_block(mat2{1,2,3,4}, 2, 0),
				mat4{0,0,0,0, 0,0,0,0, 1,2,0,0, 3,4,0,0}));
		}

		// generic matrix_stack::set_block (non-specialized type) must also accept a block
		//  that touches the last row/column: offset+size == N is valid, not only offset+size < N
		{
			using namespace cgp;
			matrix_stack<int,4,4> M; M.fill(0);
			M.set_block(matrix_stack<int,2,2>{1,2,3,4}, 2, 2);
			assert_cgp_no_msg(is_equal(M, matrix_stack<int,4,4>{
				0,0,0,0, 0,0,0,0, 0,0,1,2, 0,0,3,4}));
		}

		// single-index access matrix_stack[k1]/(k1) returns the k1-th ROW, so the valid
		//  range is [0,N1-1] even when N1>N2 (the bounds check must use N1, not N2)
		{
			using namespace cgp;
			matrix_stack<int,4,2> M = {1,2, 3,4, 5,6, 7,8};
			assert_cgp_no_msg(is_equal(M[3], numarray_stack<int,2>{7,8}));
			assert_cgp_no_msg(is_equal(M(3), numarray_stack<int,2>{7,8}));
		}

		// members added to the mat2/mat3 specializations to match the generic API
		{
			using namespace cgp;

			// remove_row_column
			mat3 const A = { 1,2,3, 4,5,6, 7,8,9 };
			assert_cgp_no_msg(is_equal(A.remove_row_column(1,1), mat2{ 1,3, 7,9 }));
			mat2 const B = { 1,2, 3,4 };
			assert_cgp_no_msg(is_equal(B.remove_row_column(0,0), matrix_stack<float,1,1>{ 4 }));

			// set_block (including a block touching the last row/column)
			assert_cgp_no_msg(is_equal(mat3().set_block(mat2{1,2,3,4}, 1, 1),
				mat3{ 0,0,0, 0,1,2, 0,3,4 }));
			assert_cgp_no_msg(is_equal(mat2().set_block(matrix_stack<float,1,1>{5}, 1, 1),
				mat2{ 0,0, 0,5 }));

			// coeff(row) and at_offset_unsafe
			assert_cgp_no_msg(is_equal(B.coeff(1), vec2{ 3,4 }));
			assert_cgp_no_msg(is_equal(B.at_offset_unsafe(2), 3.0f));
		}

		// at(k1,k2) is now checked on mat specializations (same convention as the generic)
		// coeff(k1,k2) is unchecked (direct access)
		{
			using namespace cgp;
			mat3 const M = mat3::build_identity();

			// valid at() access
			assert_cgp_no_msg(is_equal(M.at(0,0), 1.0f));
			assert_cgp_no_msg(is_equal(M.at(2,2), 1.0f));
			assert_cgp_no_msg(is_equal(M.at(0,1), 0.0f));

			// coeff gives the same value (unchecked path)
			assert_cgp_no_msg(is_equal(M.coeff(1,1), 1.0f));

			// at(k1,k2) out-of-bounds throws (CGP_ERROR_EXCEPTION defined in test build)
			auto throws_mat3 = [&]{ volatile float x = M.at(3,0); (void)x; };
			bool caught3 = false;
			try { throws_mat3(); } catch (...) { caught3 = true; }
			assert_cgp_no_msg(caught3);

			mat2 const M2 = mat2::build_identity();
			bool caught2 = false;
			try { volatile float x = M2.at(0,2); (void)x; } catch (...) { caught2 = true; }
			assert_cgp_no_msg(caught2);

			mat4 const M4 = mat4::build_identity();
			bool caught4 = false;
			try { volatile float x = M4.at(4,0); (void)x; } catch (...) { caught4 = true; }
			assert_cgp_no_msg(caught4);
		}
	}


	// ---------------------------------------------------------------------------
	//  Conformity check (mostly compile-time).
	//   require_*_api<M>() touches the whole member/static API. Instantiating it on the
	//   specializations (mat2/mat3/mat4) and on the generic template forces every type to
	//   provide the same surface; a missing method makes the build fail.
	// ---------------------------------------------------------------------------
	template <typename M>
	static void require_generic_matrix_api()
	{
		using namespace cgp;
		using T = std::decay_t<decltype(M().at_offset_unsafe(0))>;

		M m = M::build_identity();   // build_identity (static) + defined elements
		M const& c = m;

		// size / dimension / fill
		(void)m.size();
		(void)m.dimension();
		m.fill(c.at_offset_unsafe(0));

		// element access (const and non-const) — every accessor variant
		(void)c(0,0);              (void)m(0,0);
		(void)c[0];                (void)m[0];
		(void)c(0);               (void)m(0);
		(void)c.at(0,0);          (void)m.at(0,0);
		(void)c.coeff(0,0);   (void)m.coeff(0,0);
		(void)c.coeff(0);     (void)m.coeff(0);    // row accessor that had been forgotten on mat2/mat3
		(void)c.at_offset(0);     (void)m.at_offset(0);
		(void)c.at_offset_unsafe(0); (void)m.at_offset_unsafe(0);

		// iterators
		(void)m.begin(); (void)m.end();
		(void)c.begin(); (void)c.end(); (void)c.cbegin(); (void)c.cend();

		// sub-matrix manipulation
		(void)c.remove_row_column(0,0);
		m.set_block(M::build_identity(), 0, 0);   // a full block at offset 0 (needs set_block with <=)

		// free functions that must resolve for the type
		(void)norm(c);
		(void)type_str(c);
		(void)str(c);

		// scalar / matrix operators
		M r = c + c;  r = c - c;  r = -c;
		r += c; r -= c;
		r = c * T(2); r = T(2) * c; r *= T(2);
		r = c / T(2); r /= T(2);
		(void)r;
	}

	template <typename M>
	static void require_mat_specialization_api()
	{
		// build_zero exists on mat2/mat3/mat4 but not on the generic template:
		//  this catches a specialization that forgot it (as mat2 once did).
		(void)M::build_zero();
	}

	void test_matrix_conformity()
	{
		using namespace cgp;

		// specializations must expose everything the generic template exposes
		require_generic_matrix_api<mat2>();
		require_generic_matrix_api<mat3>();
		require_generic_matrix_api<mat4>();

		// reference instances of the *generic* template: float square sizes that are NOT
		//  specialized (only 2x2/3x3/4x4 are), so these exercise the generic matrix_stack itself.
		require_generic_matrix_api<matrix_stack<float,5,5>>();
		require_generic_matrix_api<matrix_stack<float,6,6>>();

		// the three specializations must also agree on their shared extra API
		require_mat_specialization_api<mat2>();
		require_mat_specialization_api<mat3>();
		require_mat_specialization_api<mat4>();
	}
}