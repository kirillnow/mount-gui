/* Copyright (c) 2015-2023 Kovshov K.A.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

/** @file vect_op.h
 *  @author Kovshov K.A. (kirillnow@gmail.com)
 *  @brief Additional operators for std::vectors (like += and <<)
 */

#ifndef KIRILLNOW_VECT_OP_H_INCLUDED
#define KIRILLNOW_VECT_OP_H_INCLUDED
//-------------------------------------------------------------------------------------------------
#include <vector>

//-------------------------------------------------------------------------------------------------
template<typename T, typename R>
inline std::vector<T>& operator<<(std::vector<T> &v, R&& r)
{
  v.emplace_back(std::forward<R>(r)); return v;
}
//-------------------------------------------------------------------------------------------------
// we have to declare 3 separate definitions of this function to over-overload <R&&> one
template<typename T, typename R>
inline std::vector<T>& operator<<(std::vector<T> &v, const std::vector<R>& r)
{
  for(auto& e:r) v.emplace_back(e);
  return v;
}
//-------------------------------------------------------------------------------------------------

template<typename T, typename R>
inline std::vector<T>& operator<<(std::vector<T> &v, std::vector<R>& r)
{
  for(auto& e:r) v.emplace_back(e);
  return v;
}
//-------------------------------------------------------------------------------------------------

template<typename T, typename R>
inline std::vector<T>& operator<<(std::vector<T> &v, std::vector<R>&& r)
{
  for(auto& e:r) v.emplace_back(std::move(e));
  r.clear(); return v;
}
//-------------------------------------------------------------------------------------------------

template<typename T, typename R>
inline std::vector<T>& operator+=(std::vector<T> &v, R&& r)
{
  return v << std::forward<R>(r);
}
//-------------------------------------------------------------------------------------------------
// operator<< on {initializer list} wouldn't work (because limits of c++ parsing).
// we have to define separate funtion because of template parameter deduction fails on {list}

namespace _vecutil { template<typename T> struct identity {  using type = T;  }; }
template<typename T> inline std::vector<T>& operator+=(std::vector<T> &v,
                                 std::initializer_list<typename _vecutil::identity<T>::type>&& r)
{
  for(auto& e:r) v.emplace_back(std::move(e));
  return v;
}
//-------------------------------------------------------------------------------------------------
template<typename T, typename R>
inline std::vector<T> operator+(const std::vector<T>& v, R&& r)
{
  std::vector<T> t{v}; return std::move(t << std::forward<R>(r));
}
//-------------------------------------------------------------------------------------------------
template<typename T, typename R>
inline std::vector<T> operator+(std::vector<T>&& v, R&& r)
{
  std::vector<T> t{std::move(v)}; return std::move(t << std::forward<R>(r));
}
//-------------------------------------------------------------------------------------------------
// in case of vector << named_variable

template<typename T> inline std::vector<T>& operator<<(std::vector<T> &v,
                                                       const std::initializer_list<T>& r)
{ v.insert(v.end(), r); return v; }

template<typename T> inline std::vector<T>& operator<<(std::vector<T> &v,
                                                       std::initializer_list<T>& r)
{ v.insert(v.end(), r); return v; }

template<typename T> inline std::vector<T>& operator<<(std::vector<T> &v,
                                                       std::initializer_list<T>&& r)
{ return v+=std::move(r); }
//-------------------------------------------------------------------------------------------------


//-------------------------------------------------------------------------------------------------
#endif // KIRILLNOW_VECT_OP_H_INCLUDED
