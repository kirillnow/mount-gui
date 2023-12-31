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

/** @file human_readable.h
 *  @brief Human readable sizes in bytes (Kb, Mb, Gb, etc...)
 *  @author Kovshov K.A. (kirillnow@gmail.com)
 */

#ifndef KIRILLNOW_HUMAN_READABLE_H_INCLUDED
#define KIRILLNOW_HUMAN_READABLE_H_INCLUDED
//-------------------------------------------------------------------------------------------------
#include <stdio.h>
#include <string>
#include <cstdint>

inline std::string human_readable_b(uint64_t num, bool i_suffix = true, int width = 0)
{
  char buff[16] = {};
  int e = (num ? 63 - __builtin_clzll(num) : 0) / 10;
  double d = (double)num / (1LL << (e*10));
  int w = width < 4 ? 5 : width; // w - actual field width, p - digits after dot.
  //w-3 if d < 99.95  99.995  99.995   99.995   (for w == {4, 5, 6, 7})
  //w-4 if d < 999.5  999.95  999.995  999.995  (w-2 if d < 9.995 for all w)
  int p = d < 9.995 ? w-2 : (d < (w == 4 ? 99.95 : 99.995)) ? w-3 :
          (d < (w == 4 ? 999.5 : w < 6 ? 999.95 : 999.995)) ? w-4 : (w-5);
  p = !e ? 0 : std::min(2, std::max(0, p));
  snprintf(buff, sizeof buff, "%*.*f %c%c", width, p, d, "\0KMGTPE"[e], "\0i"[i_suffix]);
  return buff;
}
//-------------------------------------------------------------------------------------------------

///Returns text representation of the ratio.
inline std::string human_readable_r_b(uint64_t num1, uint64_t num2, bool i_suffix = true)
{
  char buff[24] = {};
  int e1 = (num1 ? 63 - __builtin_clzll(num1) : 0) / 10;
  int e2 = (num2 ? 63 - __builtin_clzll(num2) : 0) / 10;
  double d1 = (double)num1 / (1LL << (e1*10));
  double d2 = (double)num2 / (1LL << (e2*10));
  const int w = 5; // w - actual field width, p - digits after dot.
  int p1 = d1 < 9.995 ? w-2 : (d1 < (w == 4 ? 99.95 : 99.995)) ? w-3 :
           (d1 < (w == 4 ? 999.5 : w < 6 ? 999.95 : 999.995)) ? w-4 : (w-5);
  int p2 = d2 < 9.995 ? w-2 : (d2 < (w == 4 ? 99.95 : 99.995)) ? w-3 :
           (d2 < (w == 4 ? 999.5 : w < 6 ? 999.95 : 999.995)) ? w-4 : (w-5);
  p1 = !e1 ? 0 : std::min(2, std::max(0, p1));
  p2 = !e2 ? 0 : std::min(2, std::max(0, p2));

  const char suffix[] = {"\0KMGTPE"[e1], "\0i"[i_suffix], 0, "\0KMGTPE"[e2], "\0i"[i_suffix], 0};
  if(e1 == e2)
    snprintf(buff, sizeof buff, "%.*f / %.*f %sb", p1, d1, p2, d2, suffix);
  else
    snprintf(buff, sizeof buff, "%.*f %sb / %.*f %sb", p1, d1, suffix, p2, d2, suffix+3);
  return buff;
}
//-------------------------------------------------------------------------------------------------
#endif // KIRILLNOW_HUMAN_READABLE_H_INCLUDED
