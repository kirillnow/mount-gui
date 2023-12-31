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

/** @file str.h
 *  @author Kovshov K.A. (kirillnow@gmail.com)
 *  @brief Misc string functions.
 */

#ifndef KIRILLNOW_STR_H
#define KIRILLNOW_STR_H
//-------------------------------------------------------------------------------------------------
#include <string>
#include <string_view>
#include <cctype>
#include <tuple>

//-------------------------------------------------------------------------------------------------
constexpr inline bool starts_with(std::string_view str, std::string_view prefix) noexcept
{
  return str.size() >= prefix.size() && !str.compare(0, prefix.size(), prefix);
}

constexpr inline bool ends_with(std::string_view str, std::string_view suffix) noexcept
{
  return str.size() >= suffix.size() &&
         !str.compare(str.size() - suffix.size(), suffix.size(), suffix);
}
//-------------------------------------------------------------------------------------------------

inline std::string_view trim_v(std::string_view str) noexcept
{
  const char *f = str.data(), *l = str.data() + str.size();
  for(; f != l && std::isspace(*f); ++f);
  for(; f != l && std::isspace(l[-1]); --l);
  return {f, size_t(l - f)};
}

inline std::string trim(std::string_view str) { return std::string(trim_v(str)); }
//-------------------------------------------------------------------------------------------------

inline std::string replace_suffix(std::string_view str, std::string_view old_s,
                                                        std::string_view new_s)
{
  if(!ends_with(str, old_s)) return std::string(str);
  std::string r; r.reserve(str.size() - old_s.size() + new_s.size());
  r.append(str, 0, str.size() - old_s.size()).append(new_s); return r;
}
//-------------------------------------------------------------------------------------------------

template<class Sep> inline auto split2v(std::string_view src, Sep&& sep) noexcept ->
        std::tuple<std::string_view, std::string_view, bool>

{
  if(size_t p = src.find(sep); p == std::string_view::npos) return {src, "", false};
  else return {src.substr(0, p), src.substr(p+1), true};
}
//-------------------------------------------------------------------------------------------------

template<class Sep> inline auto split2(std::string_view src, Sep&& sep) ->
        std::tuple<std::string, std::string, bool>

{
  auto [a,b,c] = split2v(src, std::forward<Sep>(sep)); return {std::string{a}, std::string{b}, c};
}
//-------------------------------------------------------------------------------------------------

//For some idiotic reason this is not part of standard library:
inline std::string operator+(const std::string& l, const std::string_view& r)
{ std::string t; t.reserve(l.size() + r.size()); t.append(l).append(r); return t; }

inline std::string operator+(const std::string_view& l, const std::string& r)
{ std::string t; t.reserve(l.size() + r.size()); t.append(l).append(r); return t; }

inline std::string operator+(std::string&& l, const std::string_view& r)
{ return std::move(l.append(r)); }

inline std::string operator+(const std::string_view& l, std::string&& r)
{ return std::move(r.insert(0, l)); }
//-------------------------------------------------------------------------------------------------
#endif // KIRILLNOW_STR_H
