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

/** @file fmt_op.h
 *  @author Kovshov K.A. (kirillnow@gmail.com)
 *  @brief Operators % and $ faciliating python-like syntax for using std::format
 */

#ifndef KIRILLNOW_FMT_OP_H
#define KIRILLNOW_FMT_OP_H
//-------------------------------------------------------------------------------------------------
#include <tuple>
#include <format>

///Operator % faciliating python-like syntax for using std::format
template<class... Args>
inline std::string operator%(std::format_string<Args...> fmt, std::tuple<Args...>&& tpl)
{
  using namespace std;
  return vformat(fmt.get(), apply(make_format_args<format_context, Args...>, move(tpl)));
}

///Constructs tuple containing mix of references and values when appropriate
template<class... Args> std::tuple<Args...> $(Args&&... args)
{ return {std::forward<Args>(args)...}; }

//-------------------------------------------------------------------------------------------------
#endif // KIRILLNOW_FMT_OP_H
