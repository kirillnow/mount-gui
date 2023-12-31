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

/** @file hires_timer.h
 *  @author Kovshov K.A. (kirillnow@gmail.com)
 *  @brief Easy to use high resolution timer.
 */

#ifndef KIRILLNOW_HIRES_TIMER_H_INCLUDED
#define KIRILLNOW_HIRES_TIMER_H_INCLUDED
//-------------------------------------------------------------------------------------------------
#include <chrono>

class hires_timer
{
  public:
    //Unfortunately, high_resolution_clock is typedef to system_clock, which is not that we want.
    //On MinGW, steady_clock calls QueryPerformanceCounter.
    using clock      = std::chrono::steady_clock;
    using time_point = clock::time_point;
    using duration   = clock::duration;
    using period     = clock::period;

    time_point value = clock::now();

    hires_timer() noexcept = default;
    hires_timer(const hires_timer&) noexcept = default;
    hires_timer(hires_timer&&) noexcept = default;
    hires_timer& operator=(const hires_timer&) noexcept = default;
    hires_timer& operator=(hires_timer&&) noexcept = default;

    hires_timer(const time_point& t) noexcept :value(t) {}

    operator const time_point&() const noexcept { return value; }
    operator       time_point&()       noexcept { return value; }

    template<class T> int64_t elapsed() const noexcept
     { return std::chrono::duration_cast<T> (hires_timer{}.value - value).count(); }

    duration reset() noexcept { auto t = value; return (*this = {}).value - t; }

    int64_t seconds()      const noexcept { return elapsed<std::chrono::seconds>();      }
    int64_t milliseconds() const noexcept { return elapsed<std::chrono::milliseconds>(); }
    int64_t microseconds() const noexcept { return elapsed<std::chrono::microseconds>(); }
    int64_t nanoseconds()  const noexcept { return elapsed<std::chrono::nanoseconds>();  }

    void swap(hires_timer& r) noexcept { std::swap(value, r.value); }
};
//-------------------------------------------------------------------------------------------------

inline hires_timer::duration operator-(const hires_timer& l, const hires_timer& r) noexcept
{ return l.value - r.value; }

inline hires_timer::duration operator+(const hires_timer& l, const hires_timer& r) noexcept \
{ return l.value + r.value; }

inline hires_timer::duration operator*(const hires_timer& l, const hires_timer& r) noexcept \
{ return l.value * r.value; }

inline hires_timer::duration operator/(const hires_timer& l, const hires_timer& r) noexcept \
{ return l.value / r.value; }

inline hires_timer::duration operator%(const hires_timer& l, const hires_timer& r) noexcept \
{ return l.value % r.value; }

namespace std { inline void swap(hires_timer& l, hires_timer& r) noexcept { l.swap(r); } }
//-------------------------------------------------------------------------------------------------

inline int64_t hours(hires_timer::duration d) noexcept
{  return std::chrono::duration_cast<std::chrono::hours>(d).count(); }

inline int64_t minutes(hires_timer::duration d) noexcept
{  return std::chrono::duration_cast<std::chrono::minutes>(d).count(); }

inline int64_t seconds(hires_timer::duration d) noexcept
{  return std::chrono::duration_cast<std::chrono::seconds>(d).count(); }

inline int64_t milliseconds(hires_timer::duration d) noexcept
{  return std::chrono::duration_cast<std::chrono::milliseconds>(d).count(); }

inline int64_t microseconds(hires_timer::duration d) noexcept
{  return std::chrono::duration_cast<std::chrono::microseconds>(d).count(); }

inline int64_t nanoseconds(hires_timer::duration d) noexcept
{  return std::chrono::duration_cast<std::chrono::nanoseconds>(d).count(); }
//-------------------------------------------------------------------------------------------------
#endif // KIRILLNOW_HIRES_TIMER_H_INCLUDED
