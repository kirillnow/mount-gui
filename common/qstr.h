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

/** @file qstr.h
 *  @author Kovshov K.A. (kirillnow@gmail.com)
 *  @brief QString<->std::string conversions.
 */

#ifndef KIRILLNOW_QSTR_H_INCLUDED
#define KIRILLNOW_QSTR_H_INCLUDED
//-------------------------------------------------------------------------------------------------
#include <string>
#include <string_view>
#include <QString>

// ///QString from std::string
// inline QString qstr(const std::string &str)
//   { return QString::fromUtf8(str.c_str(), str.size()); }

///QString from std::string_view
inline QString qstr(std::string_view str)
  { return QString::fromUtf8(str.data(), str.size()); }

///QString from const char16_t*
inline QString qstr(const char16_t* s)
 { return QString{(QChar*)s}; }

///QString from static const char16_t*
inline QString cqstr(const char16_t* s) noexcept
 { return QString::fromRawData((QChar*)s, std::char_traits<char16_t>::length(s)); }

inline QString operator ""_qstr(const char16_t* s, size_t len) noexcept
 { return QString::fromRawData((QChar*)s, len); }

///std::string from QString
inline std::string stdstr(const QString &str)
 { return str.toUtf8().toStdString(); }

//-------------------------------------------------------------------------------------------------
#endif // KIRILLNOW_QSTR_H_INCLUDED
