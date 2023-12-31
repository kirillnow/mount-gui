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

/** @file tpopen.cpp
 *  @author Kovshov K.A. (kirillnow@gmail.com)
 *  @brief Unix process read/write pseudo-async stream.
 */
//-------------------------------------------------------------------------------------------------

#include "tpopen.h"
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <poll.h>

using namespace std;
//-------------------------------------------------------------------------------------------------
bool Tp_open::open(const std::string &executable, const std::vector<std::string> &args)
{
  using ::close; using ::read; using ::write;
  if(opened()) return false;

  vector<const char*> c_args;
  for(const auto& i: args) c_args.emplace_back(i.c_str());
  c_args.push_back(nullptr);

  int p_in[2], p_out[2], p_err[2], p_sync[2];
  if(pipe2(p_in, O_CLOEXEC) || pipe2(p_out, O_CLOEXEC) ||
     pipe2(p_err, O_CLOEXEC) || pipe2(p_sync, O_CLOEXEC) )
   { s_err << "Failed to perform pipe2(2): " << strerror(errno) << ".\n"; return false; }
  if(fcntl(p_in[1], F_SETFL, O_NONBLOCK) || fcntl(p_out[0], F_SETFL, O_NONBLOCK)
                                         || fcntl(p_err[0], F_SETFL, O_NONBLOCK) )
   { s_err << "Failed to perform fcntl(2): " << strerror(errno) << ".\n"; return false; }

  proc_pid=fork();
  if(proc_pid == -1)
   { s_err << "Failed to perform fork(2): " << strerror(errno) << ".\n"; return false; }
  if(proc_pid==0)
   {
    if( dup2(p_in[0], 0)<0 || dup2(p_out[1], 1)<0 || dup2(p_err[1], 2)<0 )
     {
      const char* msg="Failed to perform dup2(2).\n";
      write(p_sync[1], "1", 1); write(p_err[1], msg, strlen(msg)); _exit(1);
     }
    if( close(p_in[0]) || close(p_out[0]) || close(p_err[0]) ||
        close(p_in[1]) || close(p_out[1]) || close(p_err[1])   )
     {
      const char* msg="Failed to perform close(2).\n";
      write(p_sync[1], "1", 1); write(2, msg, strlen(msg)); _exit(1);
     }

    execvp(executable.c_str(), (char* const*)c_args.data());
    const char* msg="Failed to perform execvp(3): ";
    const char* msg2=strerror(errno);
    write(p_sync[1], "1", 1); write(2, msg, strlen(msg));
    write(2, msg2, strlen(msg2)); write(2, "\n", 1);
    _exit(1);
   }

  if( close(p_in[0]) || close(p_out[1]) || close(p_err[1]) || close(p_sync[1]) )
   { s_err << "!Failed to perform close(2)!: " << strerror(errno) << ".\n"; return false; }
  pipe_in=p_in[1]; pipe_out=p_out[0]; pipe_err=p_err[0];

  if( read(p_sync[0], &p_sync[1], 1) > 0 )
   {
    this->sync(); waitpid(proc_pid, nullptr, 0);
    close(pipe_in); close(pipe_out); close(pipe_err); close(p_sync[0]);
    f_eof=true; /* ??? */ return false;
   }
  close(p_sync[0]);
  f_opened=true; f_eof=false;
  return true;
}
//-------------------------------------------------------------------------------------------------
int Tp_open::close()
{
  using ::close;
  if(!f_opened) return -1;

  if((pipe_in >= 0 && close(pipe_in)) || (pipe_out >= 0 && close(pipe_out)) ||
                                         (pipe_err >= 0 && close(pipe_err)))
   { s_err << "Failed to perform close(2): " << strerror(errno) << ".\n"; return -1; }
  int status=0, rt=0;
  while((rt=waitpid(proc_pid, &status, 0)) == 0);
  if( rt < 0)
   { s_err << "Failed to perform waitpid(2): " << strerror(errno) << ".\n"; return -1; }
  status = WIFEXITED(status) ? WEXITSTATUS(status) : WTERMSIG(status)+128;
  f_opened=false;
  return status;
}
//-------------------------------------------------------------------------------------------------

bool Tp_open::close_0()
{
  if(pipe_in < 0 || !::close(pipe_in)) { pipe_in = -1; return true; }
  s_err << "Failed to perform close(2): " << strerror(errno) << ".\n";
  return false;
}
//-------------------------------------------------------------------------------------------------

int Tp_open::sync()
{
  if(!f_opened) return -1;

  int rt = 0, sum = 0, count = 0, eof_cnt = 0;
  char buffer[BUFF_SZ];
  pollfd fd_pool[] = {{pipe_out, POLLIN, 0}, {pipe_err, POLLIN, 0}, {pipe_in, POLLOUT, 0}};

  s_in.clear();
  const bool do_write = (pipe_in >= 0 && s_in.peek() != char_traits<char>::eof());

  if( (rt=poll(fd_pool, do_write?3:2, timeout)) <= 0)
   {
    if(rt==0 || rt==EINTR) return 0;
    s_err << "Failed to perform poll(2): " << strerror(errno) << ".\n"; return -1;
   }

  if( fd_pool[0].revents & (POLLERR|POLLNVAL) ||
      fd_pool[1].revents & (POLLERR|POLLNVAL) || fd_pool[2].revents & POLLNVAL )
   { s_err << "Pipe polling failed.\n"; return -1; }

  s_err.clear(); s_out.clear();
  for(int i=0; i<2; ++i)
   {
    if(!fd_pool[i].revents) continue;
    while(1)
     {
      while((rt = ::read(fd_pool[i].fd, buffer, BUFF_SZ)) == -1 && errno==EINTR) ;

      if(rt > 0) { (i?s_err:s_out).write(buffer, rt); sum+=rt; continue; }
      if(rt==0) { ++eof_cnt; break; }

      if(errno == EAGAIN || errno == EWOULDBLOCK)  break;
      s_err << "Failed to perform read(2): " << strerror(errno) << ".\n"; return -1;
     }
   }
  if(fd_pool[2].revents & POLLERR)
   { ++eof_cnt; }
  else if(fd_pool[2].revents)
   {
    while( s_in.read(buffer, BUFF_SZ), (count=s_in.gcount()) > 0 )
     {
      while((rt = ::write(pipe_in, buffer, count)) == -1 && errno==EINTR) ;

      if(rt < count)  s_in.seekg(-(count-std::max(rt, 0)), ios_base::cur);
      if(rt >= 0) { sum+=rt; continue; }

      if(errno == EPIPE) { ++eof_cnt; break; }
      if(errno == EAGAIN || errno == EWOULDBLOCK)  break;
      s_err << "Failed to perform write(2): " << strerror(errno) << ".\n"; return -1;
     }
   }

  if(eof_cnt == (do_write+2)) f_eof = true;  //all 3 pipes should be closed to set eof flag.

  s_err.clear(); s_out.clear(); s_in.clear();
  return sum;
}
//-------------------------------------------------------------------------------------------------
