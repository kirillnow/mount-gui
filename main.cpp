/* Copyright (c) 2015-2023 Kovshov K.A.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

/** @file main.cpp
 *  @author Kovshov K.A. (kirillnow@gmail.com)
 */
//-------------------------------------------------------------------------------------------------

#include "common/optionparser.h"
#include "base.h"
#include "mainwindow.h"

#include <QApplication>
#include <QFile>
#include <QUuid>
#include <string>
#include <vector>
#include <iostream>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>

using namespace std;
//-------------------------------------------------------------------------------------------------

namespace option {
enum  optionIndex { UNKNOWN, HELP, XDG_CD };

static option::ArgStatus Required(const option::Option& opt, bool msg)
{
    if (opt.arg != nullptr) return option::ARG_OK;
    if (msg) cerr << "Option '" << opt.name << "' requires an argument\n";
    return option::ARG_ILLEGAL;
}

const option::Descriptor usage[] =
{
 { UNKNOWN, 0, "", "", Arg::None,
   "USAGE: mount-gui [options]\n\n" "Options:" },
 { HELP, 0, "", "help", Arg::None, "  --help  \tPrint usage and exit." },
 { XDG_CD, 0, "", "xdg-cd", Required, "  --xdg-cd \t"
   "Value of XDG_CURRENT_DESKTOP when launching with pkexec." },
 {0,0,0,0,0,0}
};
} //end namespace option
//-------------------------------------------------------------------------------------------------

std::string load_default_config() //from Qt Resource System
{
  QFile cf(":/mount-gui.conf");
  if (!cf.open(QIODevice::ReadOnly | QIODevice::Text))
    throw std::runtime_error("Failed to open Qt resource ':/mount-gui.conf'!");
  return cf.readAll().toStdString();
}
//-------------------------------------------------------------------------------------------------

std::string generate_uuid_v1() //required by wsd-probe
{ return QUuid::createUuid().toByteArray(QUuid::WithoutBraces).toStdString(); }
//-------------------------------------------------------------------------------------------------

void fix_env(const char* xdg_cd)
{
  //Desktop environments mentioned in Qt6 code:
  static const char* tbl[][2] = {{"KDE", "KDE"},     {"X-CINNAMON", "X-Cinnamon"},
                                 {"UNITY", "Unity"}, {"MATE", "MATE"},
                                 {"XFCE", "XFCE"},   {"LXDE", "LXDE"}, {"GNOME", "GNOME"}};
  if(getenv("XDG_CURRENT_DESKTOP")) return;
  string arg = xdg_cd ? xdg_cd : "";
  const char* env = "GNOME";
  for(auto& c : arg) c = toupper(c);
  for(auto& t : tbl)
    if(arg.find(t[0]) != string::npos) { env = t[1]; break; }
  setenv("XDG_CURRENT_DESKTOP", env, 0);
}
//-------------------------------------------------------------------------------------------------

int main(int argc, char *argv[])
{
  //Simplify checking for root privileges to !getuid():
  if(uid_t t[3]; !getresuid(t, t+1, t+2) && t[0] != t[1] && (!t[0] || t[1]))
    if(setresuid(0, 0, 0))
      { cerr << "setresuid(): " << s_errno() << '\n'; return 1; }

  argc-=(argc>0); argv+=(argc>0); // skip program name argv[0] if present

  option::Stats  stats(option::usage, argc, argv);
  vector<option::Option> options(stats.options_max), buffer(stats.buffer_max);
  option::Parser parse(option::usage, argc, argv, options.data(), buffer.data());

  if (parse.error())  return 1;

  if (options[option::HELP])
    { option::printUsage(std::cout, option::usage); return 0; }

//  for (int i = 0; i < parse.nonOptionsCount(); ++i)
//    { program_options.file_list.emplace_back(parse.nonOption(i)); }

  if(!getuid()) fix_env(options[option::XDG_CD].arg);

  setlocale(LC_ALL, ""); locale::global(locale(""));
  umask(022);

  char _arg0[] = "mount-gui"; //Making running this as root slightly less risky
  char* _args[] = {_arg0, nullptr};
  QApplication app(argc = 1, _args);

  MainWindow main_window;
  main_window.show();

  return app.exec();
}
