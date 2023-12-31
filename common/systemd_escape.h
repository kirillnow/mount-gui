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

/** @file systemd_escape.h
 *  @author Kovshov K.A. (kirillnow@gmail.com)
 *  @brief Escaping strings by systemd unit rules.
 */

#ifndef KIRILLNOW_SYSTEMD_ESCAPE_H_INCLUDED
#define KIRILLNOW_SYSTEMD_ESCAPE_H_INCLUDED
//-------------------------------------------------------------------------------------------------
#include <string>
#include <cctype>
#include <cwctype>
#include "ucs.h"

/// Add C-style hex escape sequence to the string
inline void append_hex_escs(std::string &akk, unsigned char code)
{
  static const char hex[]="0123456789abcdef";
  const char seq[] = {'\\', 'x', hex[code>>4], hex[code&0xF]};
  akk.append(seq, 4);
}
//-------------------------------------------------------------------------------------------------

/** From *man systemd.unit*:
 *      Basically, given a path, "/" is replaced by "-", and all other characters
 *      which are not ASCII alphanumerics are replaced by C-style "\x2d" escapes
 *      (except that "_" is never replaced and "." is only replaced when it would be
 *      the first character in the escaped path). The root directory "/" is encoded
 *      as single dash, while otherwise the initial and ending "/" are removed
 *      from all paths during transformation.
 */
inline std::string systemd_escape(const std::string &str, bool is_path)
{
  std::string res;
  if(is_path && str=="/") return "-";
  for(std::string::const_iterator i=str.begin(), i_end=str.end(); i!=i_end; ++i)
   {
     if(std::isalnum(*i) || *i == '_' || (*i == '.' && i != str.begin()))
      { res.push_back(*i); continue; }
     if(is_path && *i=='/')
      { if(i!=str.begin() && i != (i_end-1) && *(i-1) != '/') res.push_back('-'); }
     else append_hex_escs(res, *i);
   }
  return res;
}
//-------------------------------------------------------------------------------------------------

/** @brief Emulates character escape logic used in /dev/disk/by-label/
 */
inline std::string dev_disk_by_label_escape(const std::string &str)
{
  std::string res;
  res.reserve(str.size());
  for(auto f = utf8_view(str), l = f.end(); f != l; ++f)
   {
    if(f.is_valid() && (iswalnum(*f)
        || *f == '@' || *f == '#' || *f == '_' || *f == '-'
        || *f == '=' || *f == '+' || *f == ':' || *f == '.'))
     { res.append(f.base(), f.len()); }
    else
     { for(int i = 0; i < f.len(); ++i) append_hex_escs(res, f.base()[i]); }
   }
  return res;
}
//-------------------------------------------------------------------------------------------------
/** @return pair{value, true} if value is a correct systemd boolean, {false, false} otherwise. */
inline std::pair<bool,bool> systemd_bool(std::string_view value) noexcept
{
  if(value=="1" || value=="yes" || value=="true"  || value=="on")  return {true,  true};
  if(value=="0" || value=="no"  || value=="false" || value=="off") return {false, true};
  return {false, false};
}
//-------------------------------------------------------------------------------------------------
inline bool systemd_bool(std::string_view value, bool defval) noexcept
{
  auto [r, c] = systemd_bool(value); return c ? r : defval;
}
//-------------------------------------------------------------------------------------------------
inline std::string_view systemd_bool(bool value, bool onoff = false) noexcept
{
  return onoff ? (value ? "on" : "off") : (value ? "yes" : "no");
}

//-------------------------------------------------------------------------------------------------
#endif // KIRILLNOW_SYSTEMD_ESCAPE_H_INCLUDED
