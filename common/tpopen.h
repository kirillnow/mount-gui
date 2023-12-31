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

/** @file tpopen.h
 *  @author Kovshov K.A. (kirillnow@gmail.com)
 *  @brief Unix process read/write pseudo-async stream.
 */

#ifndef KIRILLNOW_TPOPEN_H_INCLUDED
#define KIRILLNOW_TPOPEN_H_INCLUDED
//-------------------------------------------------------------------------------------------------
#include <sstream>
#include <vector>
#include <string>

class Tp_open
{
  bool  f_opened = false;
  bool  f_eof = false;
  pid_t proc_pid = -1;
  int   pipe_in = -1;
  int   pipe_out = -1;
  int   pipe_err = -1;
  int   timeout = 40;  //in ms
  static constexpr int BUFF_SZ=1024;

public:
  std::stringstream s_in;
  std::stringstream s_out;
  std::stringstream s_err;

  Tp_open(const Tp_open&) = delete;
  Tp_open& operator=(const Tp_open&) = delete;
  Tp_open& operator=(Tp_open&&) = delete;

  Tp_open(Tp_open&& r) :Tp_open{} { swap(r); }
  Tp_open() = default;

  Tp_open(const std::string &executable, const std::vector<std::string> &args, int timeout_ms=40)
    { timeout=timeout_ms; open(executable, args); }
  Tp_open(const std::vector<std::string> &args, int timeout_ms=40)
    { timeout=timeout_ms; open(args.front(), args); }

  ~Tp_open() { if(f_opened) close(); }

  void swap(Tp_open& o)
  {
      using std::swap; swap(proc_pid, o.proc_pid); swap(timeout, o.timeout);
      swap(f_opened, o.f_opened); swap(f_eof, o.f_eof); swap(pipe_in, o.pipe_in);
      swap(pipe_out, o.pipe_out); swap(pipe_err, o.pipe_err);
      s_in.swap(o.s_in); s_out.swap(o.s_out); s_err.swap(o.s_err);
  }

  void clear() { Tp_open{}.swap(*this); }
  bool opened() { return f_opened; }
  bool operator!() { return !f_opened; }
  bool eof() { return f_eof; }
  void set_timeout(int timeout_ms) { timeout=timeout_ms; }

  void discard_buffers()
    { for(auto i : {&s_in, &s_out, &s_err}) {i->str(""); i->clear();} }

  ///Returns what's left unread in error stream.
  std::string err()
    { s_err.clear(); return std::string(s_err.str(), s_err.tellg()); }

  bool open(const std::vector<std::string> &args)
    { return open(args.front(), args); }

  bool open(const std::string &executable, const std::vector<std::string> &args);
  int close();
  bool close_0();
  ///Shuttle data between opened process stdio and s_in, s_out and s_err.
  int sync();

};

//-------------------------------------------------------------------------------------------------
#endif // KIRILLNOW_TPOPEN_H_INCLUDED
