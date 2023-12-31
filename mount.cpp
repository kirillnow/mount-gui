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

/** @file mount.cpp
 *  @author Kovshov K.A. (kirillnow@gmail.com)
 *  @brief Mount-related functionality.
 */
//-------------------------------------------------------------------------------------------------

#include "mount.h"
#include "base.h"
#include "common/regex.h"
#include "common/path.h"
#include "common/vect_op.h"
#include "common/execute.h"
#include "common/ucs.h"
#include "common/str.h"

#include <unordered_map>
#include <memory>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <cwctype>

using namespace std;
//-------------------------------------------------------------------------------------------------

bool contains_opt(std::string_view src, std::string_view opt) noexcept
{
  for(size_t p = 0; (p = src.find(opt, p)) != string_view::npos; p += opt.size())
    if((p == 0 || src[p-1] == ',') && (p == src.size()-opt.size() ||
                                       src[p+opt.size()] == ',' || src[p+opt.size()] == '='))
      return true;
  return false;
}
//-------------------------------------------------------------------------------------------------

string sanitize_name(string_view label)
{
  string r; r.reserve(label.size());
  auto ap = [](char c) { return c == '_' || c == '-' || c == '.'; };
  for(auto f = utf8_view(label), l = f.end(); f != l; ++f)
    if(f.is_valid() && iswalnum(*f))
      r.append(f.base(), f.len());
    else if(r.size() && !ap(r.back()))
      r.push_back((*f < 127u && ap(*f)) ? char(*f) : '_'); //inval *f == uint(-1)
  if(r.size() && ap(r.back()))
    r.pop_back();
  return r;
}
//-------------------------------------------------------------------------------------------------

std::string mount_helper::replace_placeholders(const std::string &str, const mount_info& info)
{
  string name, path = sanitize_name(info.path);
  if((name = sanitize_name(filename(info.path))).empty())
   { name = path; }
  string &lbl  = info.dev ? info.dev->at("LABEL") : name;
  string &uuid = info.dev ? info.dev->at("UUID")  : name;
  string slbl = sanitize_name(lbl);
  unordered_map<string, string> repl_hash =
   { {"d", name}, {"u", usr_info.user},
     {"D", path}, {"g", usr_info.group},
                  {"t", info.fs_type},
     {"l", slbl.empty() ? name : slbl},
     {"L", lbl.empty()  ? name : lbl},
     {"U", uuid.empty() ? name : uuid},
   };
  return regex_hash_replace(str, "%(d|D|u|g|t|l|L|U)"_re, repl_hash, 1);
}
//-------------------------------------------------------------------------------------------------

bool mount_helper::create_mountpoint(const std::string &target)
{
  const bool dont_chown = !getuid() && usr_info.uid;
  enum {OK, ACC, ISD, MKD, CHO, CMD} r = OK;
  string dir;

  struct stat stat_info;
  for(string_view _dir: path_walk{target, additive_pw})
   {
    dir = _dir;
    if(((errno = 0, r = ACC), !stat(dir.c_str(), &stat_info)) &&
       ((errno = 0, r = ISD), S_ISDIR(stat_info.st_mode)))
     { r = OK; continue; }

    if(errno != ENOENT) break; //mountpoint should be accesible for user

    if((r = MKD), !mkdir(dir.c_str(), 0755))
      if(dont_chown || ((r = CHO), !chown(dir.c_str(), usr_info.uid, usr_info.gid)))
       { log("Creating directory '" + dir + "'", ""); r = OK; continue; }

    if(!getuid() || settings.sudo_cmd.empty()) break; //no point if we already failed as root.
    vector<string> cmd = settings.sudo_cmd;
    cmd += {SYS_PREF"install", "-g", usr_info.group,
            "-dvm", "755", "-o", usr_info.user, dir};
    if((r = CMD) && !execute(cmd))
     { r = OK; continue; }
   }
  switch(r)
   {
    case ACC: log("Could not access '" + dir + "': " + s_errno());              break;
    case ISD: log("Error: '" + dir + "' is not a directory.");                  break;
    case MKD: log("Could not create directory '"    + dir + "': " + s_errno()); break;
    case CHO: log("Could not change ownership of '" + dir + "': " + s_errno()); break;
    case OK: return true; case CMD:                                             break;
   };
  return false;
}
//-------------------------------------------------------------------------------------------------

static string unit_for_mount(const std::string& path)
{
  string amu = replace_suffix(path, ".mount", ".automount");
  return filename(exists(amu) ? amu : path);
}
//-------------------------------------------------------------------------------------------------

static std::string escape_fopt(const std::string &str)
{
  static const char hex[]="0123456789abcdef";
  string res; res.reserve(str.size());
  for(unsigned char c : str)
   {
    if(isalnum(c) || c == '-' || c == '_' || c == '.')
      res.push_back(c);
    else
     { char seq[] = {'\\', '\\', 'x', hex[c>>4], hex[c&0xF]}; res.append(seq, 5); }
   }
  return res;
}
//-------------------------------------------------------------------------------------------------

static vector<string> mtp_mount_cmd(mount_info& info)
{
  vector<string> res;
  if(starts_with(info.fs_type, "fuse."))
    info.fs_type.erase(0, 5);

  if(info.fs_type == "aft-mtp-mount")
    res = {BIN_AFT_MTP, "-D", info.dev->at("_AMTP")};
  else if(info.fs_type == "simple-mtpfs")
    res = {BIN_SIMPLE_MTPFS, info.path};
  else if(info.fs_type == "jmtpfs")
    res = {BIN_JMTPFS, "-device=" + info.dev->at("_JMTP")};
  else { log("Unsupported mtpfs '" + info.fs_type + "'!"); return {}; }

  res += {info.target, "-o", info.options + &(",fsname="[info.options.empty()]) +
          info.path + ":" + escape_fopt(info.dev->at("NAME")) + ",subtype=" + info.fs_type};
  return res;
}
//-------------------------------------------------------------------------------------------------

bool mount_helper::mount(mount_info& info)
{
  info.target  = replace_placeholders(info.target,  info);
  info.options = replace_placeholders(info.options, info);

  trust_level tl = check_mountpoint(info.target);
  if(tl == TLVL_REJECT) return false;

  mount_unit* mu = find_mount_unit(system_db, info);
  const bool mu_exact = mu && mu->fstype  == info.fs_type
                           && mu->options == info.options;

  //drop root privileges if fstab entry contains 'user' instead of 'users'
  const bool user_mount = mu_exact && (getuid() ? contains_opt(info.options, "user") ||
                                                  contains_opt(info.options, "users")
                                                : contains_opt(info.options, "user"));

  const bool nosuid = contains_opt(info.options, "nosuid") &&
    contains_opt(info.options, "nodev") && !contains_opt(info.options, "suid")  &&
    !contains_opt(info.options, "dev")  && !contains_opt(info.options, "defaults");

  const bool mtp = info.dev && !info.dev->at("_MTP").empty();

  const bool drop_priv = mtp || user_mount || !info.dev || (!nosuid && !mu_exact) ||
                         tl == TLVL_ASKPASS || (tl == TLVL_SYSTEMD && !mu_exact);

  privileges_guard priv;
  if(drop_priv && tl != TLVL_TRUSTED && !priv.drop_if_feasible(usr_info))
    return false;
  if(!create_mountpoint(info.target)) return false;

  if(drop_priv && !priv.drop_if_feasible(usr_info)) return false;

  vector<string> params;
  if(getuid() && !mtp && !user_mount) params = settings.sudo_cmd;
  if(mtp)
   {
    if((params = mtp_mount_cmd(info)).empty()) return false;
   }
  else if(user_mount)
   {
    params = {SYS_PREF"mount", "-v", "--target", info.target};
   }
  else if(settings.use_systemctl && mu_exact)
   {
    params += {SYS_PREF"systemctl", "start", unit_for_mount(mu->unit_path)};
   }
  else if(settings.use_systemd_mount && !find_mount_unit(system_db, info.target))
   {
    //systemd-mount throws error if mount unit for mountpoint already exists
    params += {SYS_PREF"systemd-mount", "--discover", "-Glt",
               info.fs_type, "-o", info.options, info.path, info.target};
   }
  else params += {SYS_PREF"mount", "-vt", info.fs_type, "-o",
                  info.options, info.path, info.target};

  //TODO: handle FUSE non-root mounting
  //TODO: dump syslog tail if systemctl fails

  if(execute(params, true, true)) return false;
  //make sure automount activates
  if(settings.use_systemctl && mu_exact)
    { exists(info.target + "/."); }

  log("Device " + info.path + " has been succefully mounted.", "green");
  return true;
}
//-------------------------------------------------------------------------------------------------

bool mount_helper::unmount(mount_info& info)
{
  info.target = replace_placeholders(info.target, info);
  mount_unit* mu = find_mount_unit(system_db, info);
  const bool user_mount = mu ? contains_opt(mu->options, "user")  ||
                               contains_opt(mu->options, "users")
                             : starts_with(info.fs_type, "fuse.");
  vector<string> params;
  if(getuid() && !user_mount) params = settings.sudo_cmd;
  if(settings.use_systemd_umount && mu && !user_mount)
    params += {SYS_PREF"systemd-mount", "-Glu", info.target};
  else
    params += {SYS_PREF"umount", "-v", info.target};

  if(execute(params, true, true)) return false;

  log("Device " + info.path + " has been succefully unmounted.", "green");
  return true;
}
//-------------------------------------------------------------------------------------------------

trust_level check_mountpoint(std::string_view dir)
{
  struct deleter { void operator()(void* p) noexcept { free(p); } };
  unique_ptr<char, deleter> rp;
  if(!starts_with(dir, "/"))
   { log("Mount point should be absolute path!"); return TLVL_REJECT; }

  //I'm not sure how weakly_canonical() implements error handling, so I'm using realpath()
  for(string_view d : path_walk{dir, additive_pw})
   {
    if(char* p = realpath(string(d).c_str(), nullptr)) { rp.reset(p); continue; }
    if(errno == ENOENT) break;
    log("Can not check directory '"s + d + "': " + s_errno());
    return TLVL_REJECT;
   }
  static constexpr string_view tbl[] = { SYSTEM_DIRS };
  string_view d = rp.get();
  for(auto pref : tbl)
    if(starts_with(d, pref) && (d.size() == pref.size() || d[pref.size()] == '/'))
      return TLVL_SYSTEMD;
  return TLVL_TRUSTED;
}
//-------------------------------------------------------------------------------------------------
