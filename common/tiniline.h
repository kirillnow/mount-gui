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

/**
 * @file tiniline.h
 * @author Kovshov K.A. (kirillnow@gmail.com)
 * @brief Ini file parser.
**/

#ifndef KIRILLNOW_TINILINE_H_INCLUDED
#define KIRILLNOW_TINILINE_H_INCLUDED
//-------------------------------------------------------------------------------------------------
#include <string>

class Tini_line
{
public:
  enum ini_line_type {INI_INV, INI_EMPTY, INI_COMMENT, INI_SECTION, INI_DATA} line_type;
  std::string name, value, section, comment;

  ini_line_type operator() (std::string_view line, bool inline_comments = false)
  {
    constexpr auto npos = std::string::npos;
    size_t pos = 0, eq_pos = 0, end_pos = 0;

    pos = line.find_first_not_of(" \t\v\f\r\n");
    if(pos == npos) return line_type = INI_EMPTY;

    switch(line[pos])
     {
      case ';': case '#':
        end_pos = line.find_last_not_of(" \t\v\f\r\n")+1;
        comment.assign(line, pos+1, end_pos-pos-1);
        return line_type = INI_COMMENT;

      case '[':
        end_pos = line.find(']', pos);
        if(end_pos == npos || end_pos-pos < 2) break;
        pos = line.find_first_not_of(" \t", pos+1);
        end_pos = line.find_last_not_of(" \t", end_pos-1);
        if(end_pos < pos)  section.clear();
        else section.assign(line, pos, end_pos+1-pos);
        return line_type = INI_SECTION;

      default:
        //TODO: Multiline entries (e.g. in systemd units)
        eq_pos = line.find_first_of(";=:", pos);
        if(eq_pos == npos || eq_pos == pos || line[eq_pos] == ';') break;
        end_pos = line.find_last_not_of(" \t", eq_pos-1);
        name.assign(line, pos, end_pos+1-pos);

        pos = line.find_first_not_of(" \t", eq_pos+1);
        if(!inline_comments || (end_pos = line.find(" ;", eq_pos+1)) == npos)
          end_pos = line.find_last_not_of(" \t\v\f\r\n") + 1;
        else comment.assign(line, end_pos+2, line.size()-end_pos-2);

        if(pos == npos || end_pos <= pos) value.clear();
        else value.assign(line, pos, end_pos-pos);
        return line_type = INI_DATA;
     }
    return line_type = INI_INV;
  }
};
//-------------------------------------------------------------------------------------------------
#endif // KIRILLNOW_TINILINE_H_INCLUDED
