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

/** @file execute.h
 *  @author Kovshov K.A. (kirillnow@gmail.com)
 *  @brief Wrapper for integrating command line utilites in GUI apps.
 */

#ifndef KIRILLNOW_EXECUTE_H
#define KIRILLNOW_EXECUTE_H
//-------------------------------------------------------------------------------------------------
#include "tpopen.h"

//forward declarations; project code should contain those
void log(std::string s, const char* color);
void refresh_ui();
const std::string& exename(const std::vector<std::string>& params);
//-------------------------------------------------------------------------------------------------

inline int execute(const std::vector<std::string> &params, std::stringstream &s_out,
                   bool log_err = true, bool log_out = false, const std::string& s_in = "")
{
  Tp_open proc(params);
  if(!proc) { log(proc.err(), "red");
              log("Error: execution of " + exename(params) + " failed.", "red"); return -1; }

  size_t s_in_sz = s_in.size();
  if(s_in_sz) proc.s_in.str(s_in);

  int status = 0;
  for(; !proc.eof(); refresh_ui())
   {
    if(proc.sync() < 0) { status = -1; break; }
    if(log_out && proc.s_out.peek() != std::char_traits<char>::eof())
      for(std::string line; getline(proc.s_out, line); )
        { log(line, ""); }
    if(log_err && proc.s_err.peek() != std::char_traits<char>::eof())
      for(std::string line; getline(proc.s_err, line); )
        { log(line, "orange"); }
    if(s_in_sz)
      if(auto p = proc.s_in.tellg(); p == decltype(p)(-1) || p >= s_in_sz)
        if(!proc.close_0()) s_in_sz = 0;
   }
  status |= proc.close(); //not ||

  s_out = std::move(proc.s_out);
  s_out.seekg(0, std::ios_base::beg);
  s_out.clear(); //flags, not content

  if(log_err && status)
    log("Error: " + exename(params) + " exited with status " + std::to_string(status), "red");
  return status;
}
//-------------------------------------------------------------------------------------------------

inline int execute(const std::vector<std::string> &params, bool log_err = true,
                   bool log_out = false, const std::string& s_in = "")
{ std::stringstream tmp; return execute(params, tmp, log_err, log_out, s_in); }

//-------------------------------------------------------------------------------------------------
#endif // KIRILLNOW_EXECUTE_H
