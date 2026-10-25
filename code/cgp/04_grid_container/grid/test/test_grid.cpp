#include "cgp/01_base/base.hpp"
#include "../grid.hpp"


#include <iostream>

namespace cgp_test {

	void test_grid_2D()
	{
		{
			cgp::grid_2D<int> a;
			a.resize(2, 2);
			assert_cgp_no_msg(is_equal(a.dimension, cgp::int2{ 2,2 }));
			assert_cgp_no_msg(type_str(a)=="grid_2D<int>");

			a(0, 0) = 1; a(0, 1) = 2;
			a(1, 0) = 3; a(1, 1) = 4;
			assert_cgp_no_msg(a.data[0] == 1);
			assert_cgp_no_msg(a.data[1] == 3);
			assert_cgp_no_msg(a.data[2] == 2);
			assert_cgp_no_msg(a.data[3] == 4);

			cgp::grid_2D<int> b(2,2);
			b(0, 0) = 0; b(0, 1) = -2;
			b(1, 0) = -1; b(1, 1) = 1;

			cgp::grid_2D<int> c(2,2);
			c(0, 0) = 1; c(0, 1) = 0;
			c(1, 0) = 2; c(1, 1) = 5;
			assert_cgp_no_msg(is_equal(a + b, c));
		}

		{
			cgp::grid_2D<int> a;
			a.resize(3, 3);
			assert_cgp_no_msg(is_equal(a.dimension, cgp::int2{ 3,3 }));
			assert_cgp_no_msg(type_str(a) == "grid_2D<int>");

			a(0, 0) = 1; a(0, 1) = 2; a(0, 2) = 3;
			a(1, 0) = 4; a(1, 1) = 5; a(1, 2) = 6;
			a(2, 0) = 7; a(2, 1) = 8; a(2, 2) = 9;
			assert_cgp_no_msg(a.data[0] == 1);
			assert_cgp_no_msg(a.data[1] == 4);
			assert_cgp_no_msg(a.data[2] == 7);
			assert_cgp_no_msg(a.data[3] == 2);
			assert_cgp_no_msg(a.data[4] == 5);
			assert_cgp_no_msg(a.data[5] == 8);
			assert_cgp_no_msg(a.data[6] == 3);
			assert_cgp_no_msg(a.data[7] == 6);
			assert_cgp_no_msg(a.data[8] == 9);

			cgp::grid_2D<int> b(3, 3);
			b(0, 0) = 0; b(0, 1) = -2; b(0, 2) = 0;
			b(1, 0) = -1; b(1, 1) = 1; b(1, 2) = 3;
			b(2, 0) = 2; b(2, 1) = -2; b(2, 2) = 1;

			cgp::grid_2D<int> c(3, 3);
			c(0, 0) = 1; c(0, 1) = 0; c(0, 2) = 3;
			c(1, 0) = 3; c(1, 1) = 6; c(1, 2) = 9;
			c(2, 0) = 9; c(2, 1) = 6; c(2, 2) = 10;
			assert_cgp_no_msg(is_equal(a + b, c));
		}



		// offset round-trip 2D
		{
			assert_cgp_no_msg(cgp::offset_grid(2, 3, 4) == 14); // 2 + 4*3
			cgp::int2 idx = cgp::index_grid_from_offset(14, 4);
			assert_cgp_no_msg(idx.x == 2 && idx.y == 3);
		}

		// compound assignment operators on grid_2D
		{
			cgp::grid_2D<int> a(2,2), b(2,2);
			a(0,0)=1; a(0,1)=2; a(1,0)=3; a(1,1)=4;
			b(0,0)=5; b(0,1)=6; b(1,0)=7; b(1,1)=8;
			a += b;
			assert_cgp_no_msg(a(0,0)==6  && a(0,1)==8  && a(1,0)==10 && a(1,1)==12);
			a -= b;
			assert_cgp_no_msg(a(0,0)==1  && a(0,1)==2  && a(1,0)==3  && a(1,1)==4);
			a *= b;
			assert_cgp_no_msg(a(0,0)==5  && a(0,1)==12 && a(1,0)==21 && a(1,1)==32);
		}

		// operator-(scalar, grid_2D) must use b.dimension, not a.dimension
		{
			cgp::grid_2D<int> a(2,2);
			a(0,0)=1; a(0,1)=2; a(1,0)=3; a(1,1)=4;
			cgp::grid_2D<int> r = 10 - a;
			assert_cgp_no_msg(r(0,0)==9 && r(0,1)==8 && r(1,0)==7 && r(1,1)==6);
		}
	}


	void test_grid_3D()
	{
		{
			cgp::grid_3D<int> a;
			a.resize(2, 2, 2);
			assert_cgp_no_msg(is_equal(a.dimension, cgp::int3{ 2,2,2 }));
			assert_cgp_no_msg(type_str(a) == "grid_3D<int>");

			int counter = 0;
			for (int kx = 0; kx < 2; ++kx)
				for (int ky = 0; ky < 2; ++ky)
					for (int kz = 0; kz < 2; ++kz)
						a(kx, ky, kz) = counter++;

			
			assert_cgp_no_msg(a(0, 0, 0) == 0);
			assert_cgp_no_msg(a(0, 0, 1) == 1);
			assert_cgp_no_msg(a(0, 1, 0) == 2);
			assert_cgp_no_msg(a(1, 1, 1) == 7);


			assert_cgp_no_msg(type_str(a) == "grid_3D<int>");
		}


		// offset round-trip 3D: offset = k1 + N1*(k2 + N2*k3)
		{
			assert_cgp_no_msg(cgp::offset_grid(2, 3, 1, 3, 4) == 23); // 2+3*(3+4*1)
			cgp::int3 idx = cgp::index_grid_from_offset(23, 3, 4);
			assert_cgp_no_msg(idx.x == 2 && idx.y == 3 && idx.z == 1);
		}

		// operator+= on grid_3D must return *this
		{
			cgp::grid_3D<int> a(2,2,2); a.fill(1);
			cgp::grid_3D<int> b(2,2,2); b.fill(2);
			a += b;
			assert_cgp_no_msg(a(0,0,0)==3 && a(1,1,1)==3);
		}

		// operator-(scalar, grid_3D) must use b.dimension
		{
			cgp::grid_3D<int> a(2,2,2); a.fill(3);
			cgp::grid_3D<int> r = 10 - a;
			assert_cgp_no_msg(r(0,0,0)==7 && r(1,1,1)==7);
		}
	}

}

