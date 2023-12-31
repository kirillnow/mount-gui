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

/** @file base.h
 *  @author Kovshov K.A. (kirillnow@gmail.com)
 *  @brief Program configuration and basic operations.
 */

#ifndef CORE_H_INCLUDED
#define CORE_H_INCLUDED
//-------------------------------------------------------------------------------------------------
#include "common/str.h"
#include "common/qstr.h"
#include <functional>
#include <istream>
#include <string>
#include <vector>
#include <map>
#include "config.h"

//-------------------------------------------------------------------------------------------------
inline std::function<void(std::string, const char*)> log_fn = [](auto, auto){};

/** @brief Add one line to the log.
 * @details If color is nullptr or "", it defaults to normal text. \n added automatically. */
inline void log(std::string s, const char* color = "red") { log_fn(std::move(s), color); }
//-------------------------------------------------------------------------------------------------

struct mountopt_db_entry
{
  std::string fs_type, label, uuid; //TODO: add saving options by device path
  std::string options;
  std::string mountpoint;
};
//-------------------------------------------------------------------------------------------------

///Information about user and group.
struct user_info
{
  unsigned int uid, gid, user_gid;
  std::string user, group, user_home, user_group;
  bool fetch_current();
};
//-------------------------------------------------------------------------------------------------

/** @brief All program settings.
 */
class program_settings
{
public:
  program_settings() = default;
  program_settings& operator= (program_settings&&) = default;

  typedef std::vector<mountopt_db_entry> opt_db_t;

  std::string config_file;
  std::string use_sudo = "pkexec";
  std::vector<std::string> sudo_cmd;
  std::string mountpoint = DEFAULT_MOUNTP;
  std::string default_opts = "nodev,nosuid";
  std::string default_mtpfs;
  std::string use_mtpfs     = "auto";
  std::string nmap_networks = "auto";
  std::string use_hostname  = "auto";
  std::string hostname;
  bool use_systemctl      = false;
  bool use_systemd_mount  = false;
  bool use_systemd_umount = true;
  bool use_avahi = true;
  bool use_wsd   = true;
  bool use_nmap  = true;
  std::map<std::string, std::string> aliases;
  std::map<std::string, std::string> section_comments; //comments *before* sections
  opt_db_t options_db;
  //TODO: Save main window geometry

  mountopt_db_entry* find(const std::string &fs_type, const std::string &label,
                                                      const std::string &uuid  );
  mountopt_db_entry* find_suitable(const std::string &fs_type, const std::string &label,
                                                               const std::string &uuid  );
  void load_settings(std::istream&& src);
  void load_settings(const std::string& config_file_path, const user_info& usr_info);
  bool save_settings(const user_info& usr_info) const;

private:
  std::string ini_comment;
  void clear();
};
//-------------------------------------------------------------------------------------------------

///Utf-8 version of toupper()
std::string toupper(std::string_view src);

///Check file existence with stat(2)
bool exists(const std::string& path) noexcept;
//-------------------------------------------------------------------------------------------------

inline std::string filename(const std::string& path)
{
  size_t pos = path.find_last_of('/');
  return (pos == std::string::npos) ? path : path.substr(pos + 1);
}
//-------------------------------------------------------------------------------------------------
struct privileges_guard
{
  uint32_t groups[32] = {};
  uint32_t group_num = -1, uid = -1, gid = -1;
  ~privileges_guard() { restore(); }
  bool drop(const user_info& usr);
  ///calls drop if uid == 0 && usr.uid != 0, otherwise returns true
  bool drop_if_feasible(const user_info& usr);
  void restore();
};

//-------------------------------------------------------------------------------------------------
///Helper for unsetting flag on return from function.
struct flag_guard
{
  bool &flag;
  flag_guard(bool &f, bool val = true) :flag(f) { flag = val; }
  ~flag_guard() { flag = false; }
};

///Helper macro to prevent function reenty
#define IF_REENTRY_RETURN(...) static bool __reentry_flag = false; \
                               if(__reentry_flag) { return __VA_ARGS__; } \
                               flag_guard __reentry_guard(__reentry_flag);
//-------------------------------------------------------------------------------------------------
#define s_errno() string(strerror(errno))
//-------------------------------------------------------------------------------------------------
#endif // CORE_H_INCLUDED
