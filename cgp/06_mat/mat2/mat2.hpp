#pragma once

#include "cgp/04_grid_container/matrix_stack/matrix_stack.hpp"

// mat2 is an alias on matrix_stack<float, 2, 2>
// Its generic implementation can be found in file cgp/math/matrix/matrix_stack/matrix_stack.hpp

// A mat2 struct can be seen as
//   struct mat2 { buffer_stack< vec2, 2> }
//   (with additional functions)

namespace cgp
{
    using mat2 = matrix_stack<float, 2, 2>;

    /** Container for 2x2 matrix of floats
    *
    * Elements of mat2 are stored contiguously in stack memory as array of array
    * Elements are stored along rows
    * matrix[0]     ( xx:(0,0) xy:(0,1) )
    * matrix[1]     ( yx:(1,0) yy:(1,1) )
    **/
    template <>
    struct matrix_stack<float, 2, 2>
    {
        /** Internal storage as a 1D buffer */
        numarray_stack< numarray_stack<float, 2>, 2> data;

        // ******************************************************* //
        //  Constructors
        // ******************************************************* //
        matrix_stack();
        matrix_stack(numarray_stack<vec2, 2> const& elements);
        matrix_stack(vec2 const& row_1, vec2 const& row_2);
        matrix_stack(numarray_stack<float, 4> const& elements);
        matrix_stack(
            float xx, float xy, 
            float yx, float yy);

        // Construct from a matrix with different size.
        //  Consider the min between (N1,N1_arg) and (N2,N2_arg)
        template <int N1_arg, int N2_arg>
        explicit matrix_stack(matrix_stack<float, N1_arg, N2_arg> const& M);

        // Build as a diagonal matrix (glm compatibility)
        explicit matrix_stack(float value);
        explicit matrix_stack(float xx, float yy);

        matrix_stack(std::initializer_list<float> const& arg);
        matrix_stack(std::initializer_list<vec2> const& arg);


        // ******************************************************* //
        //  Size and fill
        // ******************************************************* //

        // Return 4
        int size() const;
        // Return {2,2}
        int2 dimension() const;
        // Fill all elements with a constant value
        mat2& fill(float value);


        // ******************************************************* //
        //  Special matrix build
        // ******************************************************* //

        // Build the identity matrix
        static mat2 build_identity();
        // Build a zero matrix
        static mat2 build_zero();

        // Build a matrix filled with a single value
        static mat2 build_constant(float value);

        // Build a diagonal matrix from a constant value or 2 floats
        static mat2 build_diagonal(float value);
        static mat2 build_diagonal(vec2 const& arg);
        static mat2 build_diagonal(float x, float y);

        // Build a scaling matrix from a constant value or 2 floats 
        //  (similar to build_diagonal)
        static mat2 build_scaling(float value);
        static mat2 build_scaling(vec2 const& arg);
        static mat2 build_scaling(float x, float y);

        // Build a rotation matrix from angle theta
        // | cos(theta) -sin(theta) |
        // | sin(theta)  cos(theta) |
        static mat2 build_rotation(float theta);



        // ******************************************************* //
        //  Get/Set
        // ******************************************************* //

        vec2 const& operator[](int k2) const;
        vec2& operator[](int k2);

        vec2 const& operator()(int k2) const;
        vec2& operator()(int k2);

        float const& operator()(int k1, int k2) const;
        float& operator()(int k1, int k2);

        inline vec2 const& at(int k2) const;
        inline vec2& at(int k2);

        inline float const& at(int k1, int k2) const;
        inline float& at(int k1, int k2);

        inline float const& coeff(int k1, int k2) const { return data.coeff(k1).coeff(k2); }
        inline float& coeff(int k1, int k2)             { return data.coeff(k1).coeff(k2); }

        inline vec2 const& coeff(int k1) const { return data.coeff(k1); }
        inline vec2& coeff(int k1)             { return data.coeff(k1); }

        inline float const& at_offset(int offset) const;
        inline float& at_offset(int offset);

        inline float const& at_offset_unsafe(int offset) const { return begin()[offset]; }
        inline float& at_offset_unsafe(int offset)             { return begin()[offset]; }

        // Remove one row and one column (returns a 1x1 matrix)
        matrix_stack<float, 1, 1> remove_row_column(int k1, int k2) const;

        // Copy a smaller block into this matrix at a given offset (default top-left)
        template <int N1_arg, int N2_arg>
        mat2& set_block(matrix_stack<float, N1_arg, N2_arg> const& block, int offset_1 = 0, int offset_2 = 0);


        /** Iterators
        *  Iterators compatible with STL syntax and std::array */
        inline float* begin();
        inline float* end();
        inline float const* begin() const;
        inline float const* end() const;
        inline float const* cbegin() const;
        inline float const* cend() const;




    };
}

namespace cgp
{
    // Construct from a matrix with different size.
    //  Consider the min between (N1,N1_arg) and (N2,N2_arg)
    template <int N1_arg, int N2_arg>
    mat2::matrix_stack(matrix_stack<float, N1_arg, N2_arg> const& M)
        :data()
    {
        int const N1m = std::min(2, N1_arg);
        int const N2m = std::min(2, N2_arg);
        for (int k1 = 0; k1 < N1m; ++k1)
            for (int k2 = 0; k2 < N2m; ++k2)
                coeff(k1, k2) = M.coeff(k1, k2);
    }

    float const& mat2::at(int k1, int k2) const
    {
        check_index_bounds(k1, k2, *this);
        return coeff(k1, k2);
    }
    float& mat2::at(int k1, int k2)
    {
        check_index_bounds(k1, k2, *this);
        return coeff(k1, k2);
    }

    vec2 const& mat2::at(int k2) const
    {
        return data.at(k2);
    }
    vec2& mat2::at(int k2)
    {
        return data.at(k2);
    }

    float const& mat2::at_offset(int offset) const
    {
        return *(begin() + offset);
    }
    float& mat2::at_offset(int offset)
    {
        return *(begin() + offset);
    }

    inline matrix_stack<float, 1, 1> mat2::remove_row_column(int idx1, int idx2) const
    {
        assert_cgp((idx1 < 2) && (idx2 < 2), "Incorrect index for removing row and column to matrix");
        matrix_stack<float, 1, 1> res;
        int k1_res = 0;
        for (int k1 = 0; k1 < 2; ++k1) {
            if (k1 != idx1) {
                int k2_res = 0;
                for (int k2 = 0; k2 < 2; ++k2) {
                    if (k2 != idx2) { res.coeff(k1_res, k2_res) = coeff(k1, k2); ++k2_res; }
                }
                ++k1_res;
            }
        }
        return res;
    }

    template <int N1_arg, int N2_arg>
    mat2& mat2::set_block(matrix_stack<float, N1_arg, N2_arg> const& block, int offset_1, int offset_2)
    {
        static_assert(N1_arg <= 2, "Block size is too large for the current matrix");
        static_assert(N2_arg <= 2, "Block size is too large for the current matrix");
        assert_cgp(N1_arg + offset_1 <= 2, "Block size exceed current matrix size");
        assert_cgp(N2_arg + offset_2 <= 2, "Block size exceed current matrix size");
        for (int k1 = 0; k1 < N1_arg; ++k1)
            for (int k2 = 0; k2 < N2_arg; ++k2)
                coeff(k1 + offset_1, k2 + offset_2) = block.coeff(k1, k2);
        return *this;
    }

    float* mat2::begin()
    {
        return &data.x.x;
    }
    float* mat2::end()
    {
        return &data.y.y+1;
    }
    float const* mat2::begin() const
    {
        return &data.x.x;
    }
    float const* mat2::end() const
    {
        return &data.y.y + 1;
    }
    float const* mat2::cbegin() const
    {
        return &data.x.x;
    }
    float const* mat2::cend() const
    {
        return &data.y.y + 1;
    }

}