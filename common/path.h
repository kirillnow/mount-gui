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

/** @file path.h
 *  @brief Functions and classes for working with directory paths.
 *  @author Kovshov K.A. (kirillnow@gmail.com)
 */

#ifndef KIRILLNOW_PATH_H_INCLUDED
#define KIRILLNOW_PATH_H_INCLUDED
//-------------------------------------------------------------------------------------------------
#include <string>

#ifndef PLATFORM_PATH_SEP
 #define PLATFORM_PATH_SEP "/\\"
 #define PLATFORM_PATH_SEP_CNT 2
#endif
#ifndef PLATFORM_PREF_SEP
 #define PLATFORM_PREF_SEP '/'
#endif
//-------------------------------------------------------------------------------------------------

/// Matches *c* with any acceptable path separators.
constexpr bool match_path_sep(char c) noexcept
{
  #if PLATFORM_PATH_SEP_CNT == 1
    return c == PLATFORM_PATH_SEP;
  #else
    for(int i = 0; i < PLATFORM_PATH_SEP_CNT; ++i)
      if(PLATFORM_PATH_SEP[i] == c) return true;
    return false;
  #endif
}
//-------------------------------------------------------------------------------------------------

namespace details {
template<bool Additive = false> class path_walk_base
{
  protected:
    std::string_view            path;
    std::string_view::iterator  it;

    path_walk_base(std::string_view path, std::string_view::const_iterator it)
      :path(path), it(it) {}

    std::string_view _substr(std::string_view::iterator f, std::string_view::iterator l) const
    { return path.substr(f - path.begin(), l - f); }

  public:
    typedef std::input_iterator_tag  iterator_category;
    typedef std::string_view         value_type;
    typedef std::string_view         reference;
    typedef value_type*              pointer;
    typedef std::streamoff           difference_type;
    typedef path_walk_base           iterator;
    typedef path_walk_base           const_iterator;

    path_walk_base(const path_walk_base&) = default;
    path_walk_base(path_walk_base&&) = default;
    path_walk_base& operator=(const path_walk_base&) = default;
    path_walk_base& operator=(path_walk_base&&) = default;

    path_walk_base() noexcept :it(path.begin()) {}

    ///Always returns *this* iterator
    path_walk_base begin() const noexcept { return *this; }

    path_walk_base end()   const noexcept { return {path, path.end()}; }

    bool operator==(const path_walk_base &o) const noexcept { return it == o.it; }
    bool operator!=(const path_walk_base &o) const noexcept { return it != o.it; }

    path_walk_base& operator++() noexcept
    {
      while(it != path.end() && !match_path_sep(*it)) ++it;
      while(it != path.end() &&  match_path_sep(*it)) ++it;
      return *this;
    }

    path_walk_base operator++(int) noexcept
    { path_walk_base tmp(*this); ++*this; return tmp; }

    std::string_view operator*() const noexcept
    {
      auto l = it; while(l != path.end() && !match_path_sep(*l)) ++l;

      return _substr(Additive ? path.begin() : it, (l != it || path.empty()) ? l : std::next(l));
    }

    bool skip_root() noexcept
    {
      if(it == path.begin() && it != path.end() && match_path_sep(*it))
        { ++(*this); return true; }
      return false;
    }
    //TODO: handle windows UNC paths
};

template<bool Store> struct path_walk_str {};
template<>           struct path_walk_str<true> { std::string storage; };

} //namespace details

inline constexpr std::true_type additive_pw;

/** @brief Helper for additive or iterative walking throught directory path.
 *
 * Example: @code
 *   string p="/usr/local/bin";
 *   for(auto i: path_walk{p, additive_pw}) cout << i << " "; cout << endl;
 *   for(auto i: path_walk{p}) cout << i << " ";
 *  @endcode
 * would print "/ /usr /usr/local /usr/local/bin \n/ usr local bin ".
 */
template<bool Additive = false, bool StoreStr = false>
class path_walk : protected details::path_walk_str<StoreStr>,
                  public    details::path_walk_base<Additive>
{
  public:
    template<class S, class... Tags, typename = std::enable_if_t<
               std::is_convertible_v<S, std::string_view> &&
               (StoreStr || !std::is_same_v<S, std::string>) &&
               sizeof...(Tags) <= size_t(Additive)>>
    path_walk(S&& src, Tags... tags) noexcept
    {
      if constexpr(StoreStr)
       { this->storage = std::move(src); this->path = this->storage; }
      else this->path = std::string_view{src};
      this->it = this->path.begin();
    }
};

template<class S> path_walk(S&& src)                       -> path_walk<false, false>;
template<class S> path_walk(S&& src,       std::true_type) -> path_walk<true,  false>;
                  path_walk(std::string&&)                 -> path_walk<false, true>;
                  path_walk(std::string&&, std::true_type) -> path_walk<true,  true>;
//-------------------------------------------------------------------------------------------------
#endif // KIRILLNOW_PATH_H_INCLUDED
