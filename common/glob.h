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

/** @file glob.h
 *  @author Kovshov K.A. (kirillnow@gmail.com)
 *  @brief Convenient C++ wrapper for glibc glob().
 */

#ifndef KIRILLNOW_GLOB_H
#define KIRILLNOW_GLOB_H
//-------------------------------------------------------------------------------------------------
#include <glob.h>
#include <string>
#include <type_traits>
#include <utility>

//-------------------------------------------------------------------------------------------------
namespace _glob_details {
  using std::begin, std::end;
  template<typename C, typename T, typename = void>
  struct is_iterable_for_convertible_type : std::false_type {};
  template<typename C, typename T>
  struct is_iterable_for_convertible_type<C, T, std::enable_if_t<
           std::is_convertible_v<decltype(*begin(std::declval<C&>())), T> >>
         : std::true_type {};

  inline const char* cstr(const char* s)        noexcept { return s; }
  inline const char* cstr(const std::string& s) noexcept { return s.c_str(); }

  /* Since error function of glob(3) doesn't accept user data argument,
     a global variable is needed to enable our wraper to accept lambda functions. */
  inline thread_local void* tlfp = nullptr;
}
//-------------------------------------------------------------------------------------------------

///Span-like container for glob() results.
class glob_result
{
  glob_t _M_glob = {};
  int    _M_res  = 0;

  public:
    using element_type    = char*;
    using value_type      = char*;
    using index_type      = size_t;
    using difference_type = std::ptrdiff_t;
    using pointer         = char**;
    using const_pointer   = const char**;
    using reference       = char*&;
    using const_reference = const char*&;
    using iterator        = pointer;
    using const_iterator  = const_pointer;

    glob_result() = default;
    glob_result(glob_result&& r) noexcept { swap(r); }
    glob_result& operator=(glob_result&& r) noexcept { swap(r); return *this; }

    glob_result& operator=(const glob_result&) = delete;
    glob_result(const glob_result&) = delete;

    glob_result(glob_t& buffer, int res) noexcept
      :_M_glob{std::exchange(buffer, glob_t{})}, _M_res{res} {}

    void swap(glob_result& r) noexcept
    { _M_glob = std::exchange(r._M_glob, _M_glob); _M_res  = std::exchange(r._M_res,  _M_res); }

    friend void swap(glob_result& l, glob_result& r) noexcept { l.swap(r);  }

    ~glob_result() noexcept { if(_M_glob.gl_pathv) globfree(&_M_glob);        }

    iterator       begin()  const noexcept { return _M_glob.gl_pathv;         }
    const_iterator cbegin() const noexcept { return (const_iterator) begin(); }
    iterator       end()    const noexcept { return begin() + size();         }
    const_iterator cend()   const noexcept { return cbegin() + size();        }
    reference      front()  const noexcept { return *begin();                 }
    reference      back()   const noexcept { return *(end() - 1);             }
    pointer        data()   const noexcept { return begin();                  }
    index_type     size()   const noexcept { return _M_glob.gl_pathc;         }
    bool           empty()  const noexcept { return !size();                  }
    int            result() const noexcept { return _M_res;                   }

    explicit operator bool() const noexcept { return !result();               }

    reference operator[](index_type i) const noexcept { return begin()[i];    }
};
//namespace std { void swap(glob_result& l, glob_result& r) noexcept { l.swap(r); } }
//-------------------------------------------------------------------------------------------------

template<typename T = std::initializer_list<const char*>,
         typename F = std::nullptr_t,
         typename = std::enable_if_t<
             (std::is_convertible_v<T, std::string> ||
              _glob_details::is_iterable_for_convertible_type<T, std::string>::value) &&
             (std::is_same_v<F, std::nullptr_t> ||
              std::is_invocable_r_v<int, F, const char*, int>)>>
glob_result glob(T&& patterns, int flags = 0, F&& err_fn = {})
{
  using namespace _glob_details;
  int (*errfp)(const char *, int) = nullptr;
  struct _S { bool f; ~_S(){ if(f) tlfp = nullptr; } } _sentry{false};

  if constexpr(!std::is_same_v<F, std::nullptr_t>)
   {
    tlfp = &err_fn; _sentry = {true};
    errfp = [](const char* p, int e) { try { return (*(decltype(&err_fn))tlfp)(p, e); }
                                       catch(...) { return 1; } };
   }
  glob_t buff{};
  int res = GLOB_NOMATCH;
  if constexpr(std::is_convertible_v<T, std::string>)
   { res = glob(cstr(patterns), flags, errfp, &buff); }
  else
    for(const auto& p : patterns)
     {
      int r = glob(cstr(p), flags, errfp, &buff);
      if(r != GLOB_NOMATCH && (res = r)) break;
      flags |= GLOB_APPEND;
     }
  return {buff, res};
}

//-------------------------------------------------------------------------------------------------
#endif // KIRILLNOW_GLOB_H
