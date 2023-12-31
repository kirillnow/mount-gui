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

/** @file regex.h
 *  @author Kovshov K.A. (kirillnow@gmail.com)
 *  @brief std::regex unique static literals and usefull functions.
 */

#ifndef KIRILLNOW_REGEX_H_INCLUDED
#define KIRILLNOW_REGEX_H_INCLUDED
//-------------------------------------------------------------------------------------------------
#include <regex>
#include <array>
#include <unordered_map>

//-------------------------------------------------------------------------------------------------
#if !defined(__cpp_nontype_template_args) || __cpp_nontype_template_args < 201911

///Literal operator that uses the type system to compile regex only once per unique string.
template<class C, C... cs> inline const std::basic_regex<C>& operator "" _re()
{
  static const std::basic_regex<C> rgx{{cs...}, std::regex::ECMAScript|std::regex::optimize};
  return rgx;
}
///Literal operator that uses the type system to compile icase regex only once per unique string.
template<class C, C... cs> inline const std::basic_regex<C>& operator "" _ire()
{
  static const std::basic_regex<C> rgx{{cs...}, std::regex::ECMAScript|
                                                std::regex::optimize|std::regex::icase};
  return rgx;
}
//-------------------------------------------------------------------------------------------------
#else // C++20 string literal operator template
template<typename C, size_t N> struct __regex_literal
{ using T=C; C s[N]{}; constexpr __regex_literal(C const(&S)[N]) { std::ranges::copy(S, s); } };

///Literal operator that uses the type system to compile regex only once per unique string.
template<__regex_literal S>
inline const std::basic_regex<typename decltype(S)::T>& operator "" _re()
{
  static const std::basic_regex<typename decltype(S)::T> rgx
    {S.s, std::regex::ECMAScript|std::regex::optimize};
  return rgx;
}

///Literal operator that uses the type system to compile icase regex only once per unique string.
template<__regex_literal S>
inline const std::basic_regex<typename decltype(S)::T>& operator "" _ire()
{
  static const std::basic_regex<typename decltype(S)::T> rgx
    {S.s, std::regex::ECMAScript|std::regex::optimize|std::regex::icase};
  return rgx;
}
#endif
//-------------------------------------------------------------------------------------------------
///Wrapper for using std::regex_iterator in range-for loops
struct re_iter
{
  const std::string& s;
  const std::regex& e;
  std::regex_constants::match_flag_type m;

  re_iter(const std::string& s, const std::regex& e,
          std::regex_constants::match_flag_type m = std::regex_constants::match_default)
    :s{s}, e{e}, m{m} {}

  re_iter(std::string&&, const std::regex&, std::regex_constants::match_flag_type = {}) = delete;

  std::sregex_iterator begin() { return {s.begin(), s.end(), e, m}; }
  std::sregex_iterator end()   { return {}; }
};

//-------------------------------------------------------------------------------------------------

inline std::string regex_hash_replace(const std::string &str, const std::regex &e,
                               const std::unordered_map<std::string,std::string> &hash,
                               int submatch=0)
{
  std::string res;
  std::string::const_iterator suffix_bgn = str.begin(),
                              suffix_end = str.end();
  for(const std::smatch& m : re_iter(str, e))
   {
    suffix_bgn = m.suffix().first;
    suffix_end = m.suffix().second;
    res.append(str, m.prefix().first - str.begin(), m.prefix().length());
    auto hval = hash.find(m[submatch].str());
    res.append( (hval == hash.end()) ? m.str() : hval->second );
   }
  res.append(str, suffix_bgn-str.begin(), suffix_end-suffix_bgn);
  return res;
}
//-------------------------------------------------------------------------------------------------

/// Calls lambda(const &match_results, &akumulator_string) on each occurrence of *e* in *str*.
template<typename C=char, typename F>
inline std::basic_string<C> lambda_replace(const std::basic_string<C> &str,
                                           const std::basic_regex<C> &e, F&& lambda)
{
  using iter_t=typename std::basic_string<C>::const_iterator;
  std::basic_string<C> res;
  auto i_end=std::regex_iterator<iter_t>();
  iter_t suffix_bgn = str.begin(),
         suffix_end = str.end();
  for(auto i=std::regex_iterator<iter_t>(str.begin(), str.end(), e); i!=i_end; ++i)
   {
    const std::match_results<iter_t> &m = *i;
    suffix_bgn = m.suffix().first;
    suffix_end = m.suffix().second;
    res.append(str, m.prefix().first-str.begin(), m.prefix().length());
    lambda(m, res);
   }
  res.append(str, suffix_bgn-str.begin(), suffix_end-suffix_bgn);
  return res;
}

#define LAMBDA_REPLACE(str, rgx, expr) \
  lambda_replace((str), (rgx), [](auto& _$, auto& akk) { expr; })

#define REGEX_REPLACE(str, rgx, expr) \
  lambda_replace((str), (rgx), [](auto& _$, auto& all) { all += (expr); })
//-------------------------------------------------------------------------------------------------
#endif // KIRILLNOW_REGEX_H_INCLUDED
