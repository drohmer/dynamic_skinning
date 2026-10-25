#pragma once

#include "cgp/01_base/base.hpp"

#include <vector>
#include <iostream>

/* ************************************************** */
/*           Header                                   */
/* ************************************************** */

namespace cgp
{

/** Dynamic-sized container for numerical data
 *
 * The numarray structure is a wrapper around an std::vector with additional convenient functionalities
 * - Overloaded operators + - * / as well as common outputs
 * - Strict bound checking with operator [] and () (unless CGP_NO_DEBUG is defined)
 *
 * Numarray follows the main syntax than std::vector
 * Elements in a numarray are stored contiguously in memory (use std::vector internally)
 *
 **/
template <typename T>
struct numarray
{
    /** Internal data stored as std::vector */
    std::vector<T> data;

    // Constructors
    numarray();                             // Empty numarray - no elements 
    numarray(int size);                     // numarray with a given size 
    numarray(std::initializer_list<T> arg); // Inline initialization using { } 
    numarray(std::vector<T> const& arg);    // Direct initialization from std::vector 

    /** Similar to matlab linespace 
    * Linear interpolation between p1 and p2 along N variable */
    static numarray<T> linespace(T const& p1, T const& p2, int N);

    /** Container size similar to vector.size() */
    int size() const;
    /** Resize container to a new size (similar to vector.resize()) */
    numarray<T>& resize(int size);
    /** Resize container to a new size, and clear it initialy to delete previous values */
    numarray<T>& resize_clear(int size);
    /** Add an element at the end of the container (similar to vector.push_back()) */
    numarray<T>& push_back(T const& value);
    /** Add an numarray of elements at the end of the container */
    numarray<T>& push_back(numarray<T> const& value);
    /** Remove all elements of the container, new size is 0 (similar to vector.clear()) */
    numarray<T>& clear();
    /** Fill the container with the same element (from index 0 to size-1) */
    numarray<T>& fill(T const& value);


    /** Element access
     * Allows numarray[i], numarray(i), and numarray.at(i)
     * Bound checking is performed unless CGP_NO_DEBUG is defined. */
    T const& operator[](int index) const;
    T& operator[](int index);
    T const& operator()(int index) const;
    T& operator()(int index);

    /** Direct access to the value - doesn't check index bounds*/
    inline T const& at(int index) const { return data[index]; }
    inline T& at(int index)             { return data[index]; }

    /** Iterators
     * Iterators on numarray are compatible with STL syntax
     * allows "forall" loops (for(auto& e : numarray) {...}) */
    typename std::vector<T>::iterator begin();
    typename std::vector<T>::iterator end();
    typename std::vector<T>::const_iterator begin() const;
    typename std::vector<T>::const_iterator end() const;
    typename std::vector<T>::const_iterator cbegin() const;
    typename std::vector<T>::const_iterator cend() const;

    /** Direct access to the value - doesn't check index bounds*/
    // Depreciated function - use at() instead
    T const& coeff(int index) const;
    T& coeff(int index);
};

template <typename T> std::string type_str(numarray<T> const&);

/** Display all elements of the numarray.*/
template <typename T> std::ostream& operator<<(std::ostream& s, numarray<T> const& v);

/** Convert all elements of the numarray to a string.
 * \param numarray: the input numarray
 * \param separator: the separator between each element 
 * \param begin/end: character added in the beginning/end of the display
 */
template <typename T> std::string str(numarray<T> const& v, std::string const& separator=" ", std::string const& begin="", std::string const& end="");

template <typename T> int size_in_memory(numarray<T> const& v);
template <typename T> auto const* ptr(numarray<T> const& v);

/** Equality check
 * Check equality (element by element) between two numarrays.
 * numarrays with different size are always considered as not equal.
 * Only approximated equality is performed for comprison with float (absolute value between floats) */
template <typename T> bool is_equal(numarray<T> const& a, numarray<T> const& b);
/** Allows to check value equality between different type (float and int for instance). */
template <typename T1, typename T2> bool is_equal(numarray<T1> const& a, numarray<T2> const& b);


template <typename T> T max(numarray<T> const& v);
template <typename T> T min(numarray<T> const& v);


/** Compute average value of all elements of the numarray.*/
template <typename T> T average(numarray<T> const& a);
template <typename T> T sum(numarray<T> const& a);


/** Math operators
 * Common mathematical operations between numarrays, and scalar or element values. */
template <typename T> numarray<T>  operator-(numarray<T> const& a);

template <typename T> numarray<T>& operator+=(numarray<T>& a, numarray<T> const& b);
template <typename T> numarray<T>& operator+=(numarray<T>& a, T const& b);
template <typename T> numarray<T>  operator+(numarray<T> const& a, numarray<T> const& b);

template <typename T> numarray<T>& operator-=(numarray<T>& a, numarray<T> const& b);
template <typename T> numarray<T>& operator-=(numarray<T>& a, T const& b);
template <typename T> numarray<T>  operator-(numarray<T> const& a, numarray<T> const& b);

template <typename T> numarray<T>& operator*=(numarray<T>& a, numarray<T> const& b);
template <typename T> numarray<T>  operator*(numarray<T> const& a, numarray<T> const& b);
template <typename T> numarray<T>& operator*=(numarray<T>& a, float b);
template <typename T> numarray<T>  operator*(numarray<T> const& a, float b);
template <typename T> numarray<T>  operator*(float a, numarray<T> const& b);

template <typename T> numarray<T>& operator/=(numarray<T>& a, numarray<T> const& b);
template <typename T> numarray<T>& operator/=(numarray<T>& a, float b);
template <typename T> numarray<T>  operator/(numarray<T> const& a, numarray<T> const& b);
template <typename T> numarray<T>  operator/(numarray<T> const& a, float b);

// Allow componentwise operations
template <typename T> numarray<T>  sub(numarray<T> const& a, T const& b);
template <typename T> numarray<T>  add(numarray<T> const& a, T const& b);
template <typename T> numarray<T>  mul(numarray<T> const& a, T const& b);
template <typename T> numarray<T>  div(numarray<T> const& a, T const& b);

template <typename T> numarray<T>  sub(T const& a, numarray<T> const& b);
template <typename T> numarray<T>  add(T const& a, numarray<T> const& b);
template <typename T> numarray<T>  mul(T const& a, numarray<T> const& b);
template <typename T> numarray<T>  div(T const& a, numarray<T> const& b);

}



/* ************************************************** */
/*           IMPLEMENTATION                           */
/* ************************************************** */

namespace cgp
{

template <typename T>
numarray<T>::numarray()
    :data()
{}

template <typename T>
numarray<T>::numarray(int size)
    :data(size)
{}

template <typename T>
numarray<T>::numarray(std::initializer_list<T> arg)
    :data(arg)
{}

template <typename T>
numarray<T>::numarray(const std::vector<T>& arg)
    :data(arg)
{}

template <typename T>
int numarray<T>::size() const
{
    return data.size();
}

template <typename T>
numarray<T>& numarray<T>::resize(int size)
{
    assert_cgp_no_msg(size>=0);
    data.resize(size);
    return *this;
}

template <typename T>
numarray<T>& numarray<T>::resize_clear(int size)
{
    clear();
    resize(size);
    return *this;
}

template <typename T>
numarray<T>& numarray<T>::push_back(T const& value)
{
    data.push_back(value);
    return *this;
}

template <typename T>
numarray<T>& numarray<T>::push_back(numarray<T> const& value)
{
    for(T const& element : value)
        data.push_back(element);
    return *this;
}

template <typename T>
numarray<T>& numarray<T>::clear()
{
    data.clear();
    return *this;
}

template <typename T>
numarray<T>& numarray<T>::fill(T const& value)
{
    int const N = size();
    for (int k = 0; k < N; ++k)
        data[k] = value;
    return *this;
}

template <typename T> std::string type_str(numarray<T> const&)
{
    using cgp::type_str;
    return "numarray<" + type_str(T()) + ">";
}


#ifndef CGP_NO_DEBUG
template <typename T>
void check_index_bounds(int index, numarray<T> const& data)
{

    int const N = data.size();
    if (index < 0 || index >= N)
        error_cgp(index_out_of_bounds_message((long long)index, (long long)N, "index", type_str(data)));
}
#else
template <typename T> void check_index_bounds(int , numarray<T> const& ) {}
#endif


template <typename T>
T const& numarray<T>::operator[](int index) const
{
    check_index_bounds(index, *this);
    return data[index];
}

template <typename T>
T& numarray<T>::operator[](int index)
{
    check_index_bounds(index, *this);
    return data[index];
}

template <typename T>
T const& numarray<T>::operator()(int index) const
{
    check_index_bounds(index, *this);
    return data[index];
}

template <typename T>
T& numarray<T>::operator()(int index)
{
    check_index_bounds(index, *this);
    return data[index];
}




template <typename T>
T const& numarray<T>::coeff(int index) const
{
    return data[index];
}

template <typename T>
T& numarray<T>::coeff(int index)
{
    return data[index];
}




template <typename T>
typename std::vector<T>::iterator numarray<T>::begin()
{
    return data.begin();
}

template <typename T>
typename std::vector<T>::iterator numarray<T>::end()
{
    return data.end();
}

template <typename T>
typename std::vector<T>::const_iterator numarray<T>::begin() const
{
    return data.begin();
}

template <typename T>
typename std::vector<T>::const_iterator numarray<T>::end() const
{
    return data.end();
}

template <typename T>
typename std::vector<T>::const_iterator numarray<T>::cbegin() const
{
    return data.cbegin();
}

template <typename T>
typename std::vector<T>::const_iterator numarray<T>::cend() const
{
    return data.cend();
}


template <typename T> std::ostream& operator<<(std::ostream& s, numarray<T> const& v)
{
    std::string const s_out = str(v);
    s << s_out;
    return s;
}
template <typename T> std::string str(numarray<T> const& v, std::string const& separator, std::string const& begin, std::string const& end)
{
    return cgp::detail::str_container(v, separator, begin, end);
}

template <typename T> int size_in_memory(numarray<T> const& v)
{
    int s = 0;
    int const N = v.size();
    for (int k = 0; k < N; ++k)
        s += cgp::size_in_memory(v[k]);
    return s;
}

template <typename T> T average(numarray<T> const& a)
{
    int const N = a.size();
    assert_cgp(N>0, "Cannot compute average on empty numarray");

    T value = {}; // assume value start at zero
    for(int k=0; k<N; ++k)
        value += a[k];
    value /= float(N);

    return value;
}
template <typename T> T sum(numarray<T> const& a) {
    int const N = a.size();
    assert_cgp(N>0, "Cannot compute sum on empty numarray");

    T value = {}; // assume value start at zero
    for(int k=0; k<N; ++k)
        value += a[k];

    return value;
}


template <typename T> T max(numarray<T> const& v)
{
    int const N = v.size();
    assert_cgp(N>0, "Cannot get max on empty numarray");

    T current_max = v[0];
    for (int k = 1; k < N; ++k) {
        T const& element = v[k];
        if(element>current_max) 
            current_max = element;
    }
        
    return current_max;
}
template <typename T> T min(numarray<T> const& v)
{
    int const N = v.size();
    assert_cgp(N>0, "Cannot get min on empty numarray");

    T current_min = v[0];
    for (int k = 1; k < N; ++k) {
        T const& element = v[k];
        if(element<current_min) 
            current_min = element;
    }
        
    return current_min;
}


template <typename T>
numarray<T>& operator+=(numarray<T>& a, numarray<T> const& b)
{
    assert_cgp(a.size()>0 && b.size()>0, "Size must be >0");
    assert_cgp(a.size()==b.size(), "Size do not agree");

    const int N = a.size();
    for(int k=0; k<N; ++k)
        a[k] += b[k];
    return a;
}



template <typename T>
numarray<T>& operator+=(numarray<T>& a, T const& b)
{
    int const N = a.size();
    for(int k=0; k<N; ++k)
        a[k] += b;
    return a;
}

template <typename T>
numarray<T>  operator+(numarray<T> const& a, numarray<T> const& b)
{
    numarray<T> res = a;
    res += b;
    return res;
}

template <typename T>
numarray<T>  operator+(numarray<T> const& a, T const& b)
{
    int const N = a.size();
    numarray<T> res(N);
    for(int k=0; k<N; ++k)
        res[k] = a[k]+b;
    return res;
}

template <typename T>
numarray<T>  operator+(T const& a, numarray<T> const& b)
{
    int const N = b.size();
    numarray<T> res(N);
    for(int k=0; k<N; ++k)
        res[k] = a+b[k];
    return res;
}

template <typename T> numarray<T>  operator-(numarray<T> const& a)
{
    int const N = a.size();
    numarray<T> b(N);
    for(int k=0; k<N; ++k)
        b[k] = -a[k];
    return b;
}


template <typename T> numarray<T>& operator-=(numarray<T>& a, numarray<T> const& b)
{
    assert_cgp(a.size()>0 && b.size()>0, "Size must be >0");
    assert_cgp(a.size()==b.size(), "Size do not agree");

    const int N = a.size();
    for(int k=0; k<N; ++k)
        a[k] -= b[k];
    return a;
}

template <typename T>
numarray<T>& operator-=(numarray<T>& a, T const& b)
{
    int const N = a.size();
    for(int k=0; k<N; ++k)
        a[k] -= b;
    return a;
}

template <typename T> numarray<T>  operator-(numarray<T> const& a, numarray<T> const& b)
{
    numarray<T> res = a;
    res -= b;
    return res;
}


template <typename T> numarray<T>& operator*=(numarray<T>& a, numarray<T> const& b)
{
    assert_cgp(a.size()>0 && b.size()>0, "Size must be >0");
    assert_cgp(a.size()==b.size(), "Size do not agree");

    const int N = a.size();
    for(int k=0; k<N; ++k)
        a[k] *= b[k];
    return a;
}
template <typename T> numarray<T>  operator*(numarray<T> const& a, numarray<T> const& b)
{
    numarray<T> res = a;
    res *= b;
    return res;
}




template <typename T> numarray<T>& operator*=(numarray<T>& a, float b)
{
    int const N = a.size();
    for(int k=0; k<N; ++k)
        a[k] *= b;
    return a;
}
template <typename T> numarray<T>  operator*(numarray<T> const& a, float b)
{
    int const N = a.size();
    numarray<T> res(N);
    for(int k=0; k<N; ++k)
        res[k] = a[k]*b;
    return res;
}
template <typename T> numarray<T>  operator*(float a, numarray<T> const& b)
{
    int const N = b.size();
    numarray<T> res(N);
    for(int k=0; k<N; ++k)
        res[k] = a*b[k];
    return res;
}

template <typename T> numarray<T>& operator/=(numarray<T>& a, numarray<T> const& b)
{
    assert_cgp(a.size()>0 && b.size()>0, "Size must be >0");
    assert_cgp(a.size()==b.size(), "Size do not agree");

    const int N = a.size();
    for(int k=0; k<N; ++k)
        a[k] /= b[k];
    return a;
}
template <typename T> numarray<T>& operator/=(numarray<T>& a, float b)
{
    assert_cgp(a.size()>0, "Size must be >0");
    const int N = a.size();
    for(int k=0; k<N; ++k)
        a[k] /= b;
    return a;
}
template <typename T> numarray<T>  operator/(numarray<T> const& a, numarray<T> const& b)
{
    numarray<T> res = a;
    res /= b;
    return res;
}
template <typename T> numarray<T>  operator/(numarray<T> const& a, float b)
{
    numarray<T> res = a;
    res /= b;
    return res;
}



template <typename T1, typename T2> bool is_equal(numarray<T1> const& a, numarray<T2> const& b)
{
    int const N = a.size();
    if(b.size()!=N)
        return false;

    using cgp::is_equal;
    for(int k=0; k<N; ++k)
        if( is_equal(a[k],b[k])==false )
            return false;
    return true;
}
template <typename T> bool is_equal(numarray<T> const& a, numarray<T> const& b)
{
    return is_equal<T,T>(a,b);
}

template <typename T>
numarray<T> numarray<T>::linespace(T const& p1, T const& p2, int N)
{
    numarray<T> buf; 
    buf.resize(N);

    T const increment = (p2 - p1) / float(N - 1);
    T current = p1;
    for (int k = 0; k < N; ++k)
    {
        buf[k] = current;
        current = current + increment;
    }
    return buf;

}

template <typename T> auto const* ptr(numarray<T> const& v)
{
    assert_cgp(v.size()>0, "Cannot get ptr of an empty numarray");
    using cgp::ptr;
    return ptr(v[0]);
}

template <typename T> numarray<T> sub(numarray<T> const& a, T const& b)
{
    int N= a.size();
    numarray<T> res;
    res.resize(N);

    for(int k=0; k<N; ++k){
        res[k] = a[k]-b;
    }

    return res;
}
template <typename T> numarray<T> add(numarray<T> const& a, T const& b)
{
    int N= a.size();
    numarray<T> res;
    res.resize(N);

    for(int k=0; k<N; ++k){
        res[k] = a[k]+b;
    }

    return res;
}
template <typename T> numarray<T> mul(numarray<T> const& a, T const& b)
{
    int N= a.size();
    numarray<T> res;
    res.resize(N);

    for(int k=0; k<N; ++k){
        res[k] = a[k]*b;
    }

    return res;
}
template <typename T> numarray<T> div(numarray<T> const& a, T const& b)
{
    int N= a.size();
    numarray<T> res;
    res.resize(N);

    for(int k=0; k<N; ++k){
        res[k] = a[k]/b;
    }

    return res;
}

template <typename T> numarray<T>  sub(T const& a, numarray<T> const& b)
{
    int N= b.size();
    numarray<T> res;
    res.resize(N);

    for(int k=0; k<N; ++k){
        res[k] = a-b[k];
    }

    return res;
}
template <typename T> numarray<T>  add(T const& a, numarray<T> const& b)
{
    int N= b.size();
    numarray<T> res;
    res.resize(N);

    for(int k=0; k<N; ++k){
        res[k] = a+b[k];
    }

    return res;
}
template <typename T> numarray<T>  mul(T const& a, numarray<T> const& b)
{
    int N= b.size();
    numarray<T> res;
    res.resize(N);

    for(int k=0; k<N; ++k){
        res[k] = a*b[k];
    }

    return res;
}
template <typename T> numarray<T>  div(T const& a, numarray<T> const& b)
{
    int N= b.size();
    numarray<T> res;
    res.resize(N);

    for(int k=0; k<N; ++k){
        res[k] = a/b[k];
    }

    return res;
}


}
