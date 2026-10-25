#include "cgp/01_base/base.hpp"
#include "../grid_stack_2D.hpp"


#include <iostream>

namespace cgp_test 
{

	void test_grid_stack_2D()
	{
		{
			cgp::grid_stack_2D<int, 2, 3> a = { 1,2,3, 4,5,6 };
			
			assert_cgp_no_msg(a(0, 0) == 1); assert_cgp_no_msg(a(0, 1) == 2); assert_cgp_no_msg(a(0, 2) == 3);
			assert_cgp_no_msg(a(1, 0) == 4); assert_cgp_no_msg(a(1, 1) == 5); assert_cgp_no_msg(a(1, 2) == 6);

			assert_cgp_no_msg( cgp::is_equal(cgp::element<0, 0>(a), 1) ); assert_cgp_no_msg(cgp::is_equal(cgp::element<0, 1>(a), 2)); assert_cgp_no_msg(cgp::is_equal(cgp::element<0, 2>(a), 3));
			assert_cgp_no_msg( cgp::is_equal(cgp::element<1, 0>(a), 4)); assert_cgp_no_msg(cgp::is_equal(cgp::element<1, 1>(a), 5)); assert_cgp_no_msg(cgp::is_equal(cgp::element<1, 2>(a), 6));

			assert_cgp_no_msg(type_str(a) == "grid_stack_2D<int,2,3>");
		}

		{
			using namespace cgp;
			grid_stack_2D<int,2,3> const a = {1,2,3, 4,5,6};
			cgp::is_equal(a(1,2), 6);
			cgp::is_equal(a({1,2}), 6);
			cgp::is_equal(a[{1,2}], 6);

		}


		// index_grid_from_offset_stack must be the inverse of offset_grid_stack
		{
			// offset_grid_stack<5>(3,2) = 3 + 5*2 = 13
			assert_cgp_no_msg(cgp::offset_grid_stack<5>(3,2) == 13);
			auto [k1, k2] = cgp::index_grid_from_offset_stack<5>(13);
			assert_cgp_no_msg(k1 == 3 && k2 == 2);
		}

		// operator-= must subtract (was mistakenly using +=)
		{
			cgp::grid_stack_2D<int,2,3> a = {10,8,6, 4,2,0};
			cgp::grid_stack_2D<int,2,3> b = { 1,2,3, 4,5,6};
			a -= b;
			assert_cgp_no_msg(a(0,0)==9  && a(0,1)==6  && a(0,2)==3);
			assert_cgp_no_msg(a(1,0)==0  && a(1,1)==-3 && a(1,2)==-6);
		}
	}
}