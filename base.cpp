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

/** @file base.cpp
 *  @author Kovshov K.A. (kirillnow@gmail.com)
 *  @brief Program configuration and basic operations.
 */
//-------------------------------------------------------------------------------------------------

#include "base.h"
#include "common/ucs.h"
#include "common/regex.h"
#include "common/tiniline.h"
#include "common/systemd_escape.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <pwd.h>
#include <grp.h>

std::string load_default_config();

using namespace std;
//-------------------------------------------------------------------------------------------------
static std::string dirname(const std::string& path)
{
  size_t pos = path.find_last_of('/');
  return (pos == string::npos) ? "." : (!pos ? "/" : path.substr(0, pos));
} 
//-------------------------------------------------------------------------------------------------

static vector<string> approved_sudo_cmd(const string& sudo)
{
  static constexpr initializer_list<const char*> sudo_list[] = { APPROVED_SUDO_CMDS };
  for(auto& l : sudo_list)
    if(ends_with(*l.begin(), "/" + sudo))
      return {l.begin(), l.end()};
  return {};
}
//-------------------------------------------------------------------------------------------------

static string select_mtpfs(const string& mtpfs)
{
  static constexpr string_view tbl[] = {BIN_AFT_MTP, BIN_SIMPLE_MTPFS, BIN_JMTPFS};
  const bool noauto = !mtpfs.empty() && mtpfs != "auto";
  for(string_view x : tbl)
    if(noauto ? ends_with(x, "/" + mtpfs) : exists(string{x}))
      return string{x.substr(x.rfind('/') + 1)};
  return {};
}
//-------------------------------------------------------------------------------------------------

const std::string& exename(const vector<string> &params)
{
  auto sudo = approved_sudo_cmd(filename(params.at(0)));
  return params.at(sudo.empty() ? 0 : sudo.size());
}
//-------------------------------------------------------------------------------------------------

static bool create_config_file_if_necessary(const std::string& path)
{
  string dir = dirname(path);
  if(!exists(dir))  //expose all errors
   {
    if(mkdir(dir.c_str(), S_IRWXU))
     { log("Could not create directory '" + dir + "': " + s_errno()); return false; }
   }
  if(!exists(path)) //expose all errors
   {
    int fd = open(path.c_str(), O_WRONLY|O_CREAT|O_TRUNC, 0600);
    if(fd < 0) { log("Could not create file '" + path + "': " + s_errno()); return false; }
    close(fd);
   }
  return true;
}
//-------------------------------------------------------------------------------------------------

mountopt_db_entry* program_settings::find(const std::string& fs_type,
                                          const std::string& label,
                                          const std::string& uuid)
{
  for(auto& x : options_db)
    if(fs_type == x.fs_type && label == x.label && uuid == x.uuid)
      return &x;
  return nullptr;
}
//-------------------------------------------------------------------------------------------------

mountopt_db_entry* program_settings::find_suitable(const std::string& fs_type,
                                                   const std::string& label,
                                                   const std::string& uuid)
{
  if(auto r = find(fs_type, label, uuid)) return r; // search by combined params
  if(auto r = find(fs_type, "",    uuid)) return r; // search by UUID
  if(auto r = find(fs_type, label, ""))   return r; // search by Label
  if(auto r = find(fs_type, "",    ""))   return r; // search by FS type
  return nullptr;
}
//-------------------------------------------------------------------------------------------------
template<class... Ts> static bool set_named_opt(const string& name, string&& val,
                                                initializer_list<const char*> names, Ts&... opts)
{
  constexpr bool fa[] = {!std::is_same_v<Ts, bool>...};
  void* const pa[] = { &opts... };
  if(names.size() != sizeof...(opts)) throw std::out_of_range("set_opt()");
  for(auto& x : pa)
   {
    if(names.begin()[&x-pa] != name) continue;
    if(fa[&x-pa]) *(string*)x = std::move(val);
    else          *(bool*)x   = systemd_bool(val, *(bool*)x);
    return true;
   }
  return false;
}
//-------------------------------------------------------------------------------------------------
void program_settings::load_settings(std::istream&& src)
{
  enum { GENERAL, NETSCAN, ALIASES, OPTIONS, OTHER } section;
  Tini_line ini;
  clear();
  for(string line; getline(src, line);) switch(ini(line))
   {
    case Tini_line::INI_INV:
    case Tini_line::INI_EMPTY:   break;
    case Tini_line::INI_COMMENT:
      ini_comment += ';' + ini.comment + '\n'; break;

    case Tini_line::INI_SECTION:
      if(ini.section == "General")      section = GENERAL;
      else if(ini.section == "Netscan") section = NETSCAN;
      else if(ini.section == "Aliases") section = ALIASES;
      else if(starts_with(ini.section, "Options/"))
       { section = OPTIONS; options_db.emplace_back(); }
      else { section = OTHER; break; }
      section_comments.emplace(std::move(ini.section), std::move(ini_comment));
      break;

    case Tini_line::INI_DATA:
      bool ok = true;
      switch(section)
       {
        case ALIASES:
          aliases.emplace(std::move(ini.name), std::move(ini.value));              break;
        case GENERAL:
          ok = set_named_opt(ini.name, std::move(ini.value),
                             {"UseSudo", "DefaultOptions", "Mountpoint", "UseSystemctl",
                              "UseSystemdMount", "UseSystemdUmount", "UseMtpfs"},
                             use_sudo, default_opts, mountpoint, use_systemctl,
                             use_systemd_mount,   use_systemd_umount, use_mtpfs);  break;
        case NETSCAN:
          ok = set_named_opt(ini.name, std::move(ini.value),
                             {"Hostname", "UseAvahi", "UseWSD",
                              "UseNmap", "NmapNetworks"},
                             use_hostname, use_avahi, use_wsd,
                             use_nmap, nmap_networks);                             break;
        case OPTIONS:
          auto& e = options_db.back();
          ok = set_named_opt(ini.name, std::move(ini.value),
                             {"FS", "Label", "UUID", "Mountpoint", "Options"},
                             e.fs_type, e.label, e.uuid, e.mountpoint, e.options); break;
       }
      if(!ok) log("Unknown option '" + ini.name + "' in section '" + ini.section + "'!");
   }
  if(!use_sudo.empty()) sudo_cmd = approved_sudo_cmd(use_sudo);
  default_mtpfs = select_mtpfs(use_mtpfs);
  if(!regex_match(nmap_networks, "auto|(?:(?:(?:25[0-5]|(?:2[0-4]|1\\d|[1-9]|)\\d)\\.){2}"
                                 "(?:(?:25[0-5]|(?:2[0-4]|1\\d|[1-9]|)\\d)"
                                 "(?:-(?:25[0-5]|(?:2[0-4]|1\\d|[1-9]|)\\d))?\\.?){2}"
                                 "(?:\\/(?:1[6-9]|2[0-4]))?\\s*|ipv6-link-local\\s*)+|"_re))
   {
    section_comments["Netscan"] += "; NmapNetworks=" + nmap_networks + '\n';
    nmap_networks = "auto"; log("Incorrect value for NmapNetworks setting!");
   }
}
//-------------------------------------------------------------------------------------------------

void program_settings::load_settings(const std::string& config_file_path,
                                     const user_info& usr_info)
{
  privileges_guard priv;
  config_file = config_file_path;
  ifstream file;
  if(exists(config_file) && priv.drop_if_feasible(usr_info))
   {
    if(file.open(config_file), file)
      load_settings(std::move(file));
    else
      log("Failed to open config file '" + config_file + "'");
   }
  else load_settings(istringstream{load_default_config()});
}
//-------------------------------------------------------------------------------------------------

bool program_settings::save_settings(const user_info& usr_info) const
{
  privileges_guard priv;
  if(!priv.drop_if_feasible(usr_info)) return false;

  if(!create_config_file_if_necessary(config_file)) return false;

  ofstream file(config_file, ios::out|ios::trunc);
  if(!file)
   { log("Could not open config file '" + config_file + "'."); return false; }

  auto comment = section_comments.find("General");
  if(comment != section_comments.end() && !comment->second.empty())
    file << comment->second;

  file << "[General]\n" << "UseSudo" << '=' << use_sudo                 << '\n'
       << "Mountpoint"       << '=' << mountpoint                       << '\n'
       << "DefaultOptions"   << '=' << default_opts                     << '\n'
       << "UseSystemctl"     << '=' << systemd_bool(use_systemctl)      << '\n'
       << "UseSystemdMount"  << '=' << systemd_bool(use_systemd_mount)  << '\n'
       << "UseSystemdUmount" << '=' << systemd_bool(use_systemd_umount) << '\n'
       << "UseMtpfs"         << '=' << use_mtpfs                        << "\n\n";

  comment = section_comments.find("Netscan");
  if(comment != section_comments.end() && !comment->second.empty())
    file << comment->second;

  file << "[Netscan]\n" << "Hostname" << '=' << use_hostname << '\n'
       << "UseAvahi"     << '=' << systemd_bool(use_avahi)   << '\n'
       << "UseWSD"       << '=' << systemd_bool(use_wsd)     << '\n'
       << "UseNmap"      << '=' << systemd_bool(use_nmap)    << '\n'
       << "NmapNetworks" << '=' << nmap_networks             << "\n\n";

  comment = section_comments.find("Aliases");
  if(comment != section_comments.end() && !comment->second.empty())
    file << comment->second;

  file << "[Aliases]\n";
  for(auto& i: aliases) file << i.first << "=" << i.second << "\n";

  int sct_num = 0;
  for(auto& i: options_db)
   {
    string sct_name="Options/" + to_string(++sct_num);
    comment = section_comments.find(sct_name);
    if(comment != section_comments.end() && !comment->second.empty())
      file << comment->second;

    file << "[" + sct_name + "]\n";
    if(!i.fs_type.empty()) file << "FS"    << '=' << i.fs_type << "\n";
    if(!i.label.empty())   file << "Label" << '=' << i.label   << "\n";
    if(!i.uuid.empty())    file << "UUID"  << '=' << i.uuid    << "\n";
    file << "Options"    << '=' << i.options                   << "\n";
    file << "Mountpoint" << '=' << i.mountpoint                << "\n";
   }
  if(!ini_comment.empty()) file << ini_comment << "\n";
  return true;
}
//-------------------------------------------------------------------------------------------------

void program_settings::clear()
{
  use_sudo.clear(); sudo_cmd.clear();   mountpoint = DEFAULT_MOUNTP;
  aliases.clear();  options_db.clear(); section_comments.clear();
  ini_comment.clear();
}
//-------------------------------------------------------------------------------------------------

bool user_info::fetch_current()
{
  uid = getuid(), gid = getgid();
  const char *sudo_uid = nullptr, *sudo_gid = nullptr;

  if(!uid && ((sudo_uid = getenv("SUDO_UID")) || (sudo_uid = getenv("PKEXEC_UID"))))
   {
    if((uid = strtoul(sudo_uid, 0, 0)) && uid < (uid_t)-1)
     {
      if((sudo_gid = getenv("SUDO_GID")))
       {
        unsigned g = strtoul(sudo_gid, 0, 0);
        if(g && g < (gid_t)-1) gid = g;
        else sudo_gid = nullptr;
       }
     }
    else { uid = 0; sudo_uid = nullptr; }
   }

  struct passwd *pwdent=nullptr;
  struct group  *grpent=nullptr;

  if( errno=0, (pwdent = getpwuid(uid)) == nullptr)
     { return false; }
  user      = pwdent->pw_name;
  user_gid  = pwdent->pw_gid;
  user_home = pwdent->pw_dir;

  if(sudo_uid && !sudo_gid) gid = user_gid;

  if( errno=0, (grpent = getgrgid(gid)) == nullptr)
   { return false; }
  group = grpent->gr_name;
  if(gid == user_gid) { user_group = group; return true; }

  if( errno=0, (grpent = getgrgid(user_gid)) == nullptr)
  { return false; }
  user_group = grpent->gr_name;
  return true;
}
//-------------------------------------------------------------------------------------------------

bool privileges_guard::drop(const user_info &usr)
{
  static_assert(sizeof(gid_t) == sizeof groups[0]);
  uid_t u0 = getuid(); gid_t g0 = getgid(); int n0;

  if((n0 = getgroups(std::size(groups), groups)) < 0)
   { log("Failed to read user groups: " + s_errno()); return false; }
  if(!initgroups(usr.user.c_str(), usr.user_gid)) group_num = n0;
  else { log("Failed to initialize user groups: " + s_errno()); return false; }
  if(!setresgid(usr.gid, usr.gid, -1)) gid = g0;
  else { log("Failed to set effective group id: " + s_errno()); return false; }
  if(!setresuid(usr.uid, usr.uid, -1)) uid = u0;
  else { log("Failed to set effective user id: " + s_errno()); return false; }
  return true;
}
//-------------------------------------------------------------------------------------------------

bool privileges_guard::drop_if_feasible(const user_info &usr)
{
  return getuid() || !usr.uid || drop(usr);
}
//-------------------------------------------------------------------------------------------------

void privileges_guard::restore()
{
  if(uid != -1) { if(!setresuid(uid, uid, -1)) uid = -1;
                  else log("Failed to set effective user id: " + s_errno());  }
  if(gid != -1) { if(!setresgid(gid, gid, -1)) gid = -1;
                  else log("Failed to set effective group id: " + s_errno()); }

  if(group_num != -1) { if(!setgroups(group_num, groups)) group_num = -1;
                        else log("Failed to set user groups: " + s_errno());  }
}
//-------------------------------------------------------------------------------------------------

std::string toupper(std::string_view src)
{
  string r; r.reserve(src.size());
  char buff[4] = {}; char32_t u32b;
  for(auto f = utf8_view(src), l = f.end(); f != l; ++f)
   {
    if(*f < 128) { r.push_back(toupper(*f)); continue; }
    r.append(buff, utf32_to_utf8(&(u32b = toupper(*f)), 1, buff));
   }
  return r;
}
//-------------------------------------------------------------------------------------------------

bool exists(const std::string &path) noexcept
{
  struct stat st{}; return !stat(path.c_str(), &st);
}
//-------------------------------------------------------------------------------------------------
