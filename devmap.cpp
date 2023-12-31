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

/** @file devmap.cpp
 *  @author Kovshov K.A. (kirillnow@gmail.com)
 *  @date 2016-2017,2023
 *  @brief Mapping devices and system configuration
 */
//-------------------------------------------------------------------------------------------------

#include "devmap.h"
#include "base.h"
#include "common/execute.h"
#include "common/glob.h"
#include "common/tiniline.h"
#include "common/fmt_op.h"
#include "common/human_readable.h"
#include <fstream>
#include <charconv>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <libmtp.h>

static constexpr char lsblk_columns[]="NAME,PATH,PKNAME,FSTYPE,SIZE,TYPE,HOTPLUG,"
                                      "RM,LABEL,PARTLABEL,PARTUUID,MODEL,SERIAL,UUID";
using namespace std;
//-------------------------------------------------------------------------------------------------
static void add_mtp_devices(device_map& out);

device_info devmap_init_columns()
{
  return { {"PATH", ""}, {"NAME",  ""}, {"MOUNTPOINT", ""}, {"PARTUUID", ""},
           {"UUID", ""}, {"LABEL", ""}, {"PARTLABEL",  ""}, {"PKNAME",   ""},
           {"TYPE", ""}, {"SIZE",  ""}, {"SERIAL",     ""}, {"MODEL",    ""},
           {"_MTP", ""}, {"RM",    ""}, {"FSTYPE",     ""}, {"_NETDEV",  ""}, };
}
//-------------------------------------------------------------------------------------------------

std::string unescape_hex(std::string_view str)
{
  string::size_type prev_pos = 0, pos = 0;
  unsigned char v;
  string res; res.reserve(str.size());
  while( (pos = str.find("\\x", prev_pos, 2)) != string::npos )
   {
    if(str.size() - pos < 4) break;
    res.append(str, prev_pos, pos - prev_pos);
    if(from_chars(&str[pos+2], &str[pos+4], v, 16).ec == std::errc{})
      res.push_back((char)v);
    else res.push_back(' ');
    prev_pos = pos + 4;
   }
  res.append(str, prev_pos, str.size() - prev_pos);
  return res;
}
//-------------------------------------------------------------------------------------------------

bool extract_nameval_pair(const string &str, size_t &pos, string &name, string &value)
{
  if(pos == string::npos) return false;
  pos = str.find_first_not_of(" \t\r\n", pos);
  if(pos == string::npos) return false;
  size_t prev_pos = pos;
  pos = str.find_first_of(" =\t\r\n", pos);
  if(pos == string::npos) return false;
  name.assign(str, prev_pos, pos - prev_pos);

  pos = str.find_first_not_of(" \t\r\n", pos);
  if( pos == string::npos || str[pos] != '=' ) return false;
  pos = str.find_first_not_of(" \t\r\n", pos+1);
  if(pos == string::npos) return false;

  if(str[pos] == '"')
   {
    if(pos == str.size() - 1) return false;
    prev_pos = ++pos;
    pos=str.find_first_of('"', pos);
    if(pos == string::npos) return false;
    value.assign(str, prev_pos, pos-prev_pos); ++pos;
   }
  else
   {
    prev_pos = pos;
    pos = str.find_first_of(" \t\r\n", pos);
    if(pos == string::npos) pos = str.size();
    value.assign(str, prev_pos, pos - prev_pos);
   }
  return true;
}
//-------------------------------------------------------------------------------------------------
static pair<string,string> split_netdev_path(const string& path)
{
  //for non-smb paths split on the last ':' before '/'
  size_t p1, p2, p3;
  if((p1 = path.find_first_not_of("/\\")) == string::npos)
    return {path,path};
  p2 = path.find_first_of("/\\", p1 + 1);
  if(!p1 && (p3 = path.rfind(':', p2)) != string::npos) p2 = p3;
  if(p2 == string::npos) p2 = path.size();
  p3 = p2;
  if(p2 && path[p1] == '[' && path[p2 - 1] == ']') { ++p1; --p2; }
  return {path.substr(p1, p2 - p1), path.substr(p3)};
}
//-------------------------------------------------------------------------------------------------

static bool add_mount_options_and_netdevs(device_map& dmap)
{
  stringstream s_out;
  if(execute({SYS_PREF"findmnt", "-UPo", "SOURCE,SIZE,FSTYPE,TARGET,OPTIONS"}, s_out))
    return false;

  string line, name, val, src, sz, fstype, tgt, opt;
  pair<string*, string_view> tie[] =
   { {&src, "SOURCE"}, {&sz, "SIZE"}, {&fstype, "FSTYPE"}, {&tgt, "TARGET"}, {&opt, "OPTIONS"} };
  while(getline(s_out, line))
   {
    for(size_t pos = 0; extract_nameval_pair(line, pos, name, val);)
      for(auto& t : tie)
        if(t.second == name) *t.first = unescape_hex(val);

    if(device_info* d = find_device(dmap, src))
     {
      (*d)["MOUNTPOINT"] = std::move(tgt); (*d)["OPTIONS"] = std::move(opt);
     }
    else if(is_netdev(src, fstype, opt))
     {
      auto &dev = (dmap.emplace_back(devmap_init_columns()), dmap.back());
      auto [srvr, shr] = split_netdev_path(src);
      for(char& c : shr) if(c == '\\') c = '/';
      dev["SIZE"]    = std::move(sz);  dev["FSTYPE"]     = std::move(fstype);
      dev["OPTIONS"] = std::move(opt); dev["MOUNTPOINT"] = std::move(tgt);
      dev["NAME"]    = std::move(shr); dev["PATH"]       = std::move(src);
      dev["_NETDEV"] = "1";            dev["PKNAME"]     = std::move(srvr);
     }
    else if(starts_with(fstype, "fuse.") && fstype.find("mtp", 5) != string::npos)
     {
      auto &dev = (dmap.emplace_back(devmap_init_columns()), dmap.back());
      if(auto [l,r,s] = split2v(src, ':'); s && starts_with(l, "/dev/bus/usb"))
       { dev["PATH"] = l; dev["NAME"] = unescape_hex(r); }
      else { dev["PATH"] = dev["NAME"] = std::move(src); }

      dev["SIZE"]    = std::move(sz);  dev["FSTYPE"]     = std::move(fstype);
      dev["OPTIONS"] = std::move(opt); dev["MOUNTPOINT"] = std::move(tgt);
      dev["_MTP"]    = "*";
     }
   }
  return true;
}
//-------------------------------------------------------------------------------------------------

static void add_preconfigured_netdevs(device_map& dmap, const mount_db db)
{
  for(auto& mu : db)
    if(mu.what_sel == mount_unit::PATH && !find_device(dmap, mu.what) &&
       is_netdev(mu.what, mu.fstype, mu.options))
     {
      auto &dev = (dmap.emplace_back(devmap_init_columns()), dmap.back());
      auto [srvr, shr] = split_netdev_path(mu.what);
      for(char& c : shr) if(c == '\\') c = '/';
      dev["OPTIONS"] = mu.options; dev["FSTYPE"] = mu.fstype;
      dev["PATH"]    = mu.what;    dev["NAME"]   = std::move(shr);
      dev["_NETDEV"] = "1";        dev["PKNAME"] = std::move(srvr);
     }
}
//-------------------------------------------------------------------------------------------------

device_map system_scan_devices(const mount_db& system_db)
{
  device_map res;
  stringstream s_out;
  if(execute({SYS_PREF"lsblk", "-nPo", lsblk_columns}, s_out))
    return res;

  string line, name, value;
  while(getline(s_out, line))
   {
    res.emplace_back(devmap_init_columns());
    auto &dev = res.back();
    for(size_t pos = 0; extract_nameval_pair(line, pos, name, value);)
      dev[name] = unescape_hex(value);

    if(dev["RM"] == "0" && dev["HOTPLUG"] == "1") dev["RM"] = "USB";
   }
  add_mtp_devices(res);
  add_mount_options_and_netdevs(res);
  add_preconfigured_netdevs(res, system_db);
  return res;
}
//-------------------------------------------------------------------------------------------------

mount_db scan_systemd_units()
{
  //`systemd-analyze unit-paths`; order matters
  auto mu = {"/etc/systemd/system.control/*.mount", "/run/systemd/system.control/*.mount",
             "/run/systemd/transient/*.mount",      "/run/systemd/generator.early/*.mount",
             "/etc/systemd/system/*.mount",         "/etc/systemd/system.attached/*.mount",
             "/run/systemd/system/*.mount",         "/run/systemd/system.attached/*.mount",
             "/run/systemd/generator/*.mount",      "/usr/local/lib/systemd/system/*.mount",
           /*"/usr/lib/systemd/system/ *.mount",*/  "/run/systemd/generator.late/*.mount",  };
  mount_db res;
  for(char* p : glob(mu))
   {
    if(struct stat st{}; lstat(p, &st) || S_ISLNK(st.st_mode)) continue;
    if(mount_unit unit{}; read_systemd_unit(p, unit))
      res.emplace_back(std::move(unit));
   }
  return res;
}
//-------------------------------------------------------------------------------------------------

static string resolve_dev_link(const string& src)
{
  ssize_t l; char b[128]{};
  if(starts_with(src, "/dev/disk/") && (l = readlink(src.c_str(), b, 128)) > 0)
    return "/dev/" + filename(string(b, l));
  return src;
}
//-------------------------------------------------------------------------------------------------

bool read_systemd_unit(const std::string &path, mount_unit &out)
{
  Tini_line ini;
  bool mount_section = false;
  ifstream file{path};
  for(string l; getline(file, l); )
   {
    if(ini(l) == Tini_line::INI_INV) return false;
    if(ini.line_type == Tini_line::INI_SECTION)
      mount_section = (ini.section == "Mount");
    if(ini.line_type != Tini_line::INI_DATA || !mount_section) continue;
    if(ini.name == "Options")  { out.options = std::move(ini.value); continue; }
    if(ini.name == "Type")     { out.fstype  = std::move(ini.value); continue; }
    if(ini.name == "Where")    { out.where   = std::move(ini.value); continue; }
    if(ini.name != "What") continue;
    if(ini.value.find('=') != string::npos)
     {
      Tini_line l2; l2(ini.value);
      out.what_sel = mount_unit::LABEL;
      for(const char* s : {"LABEL", "UUID", "PARTLABEL", "PARTUUID"})
       { if(s == l2.name) { out.what = std::move(l2.value); break; } out.what_sel++; }
      if(out.what_sel < mount_unit::INV_SEL) continue;
     }
    out.what = resolve_dev_link(ini.value);
   }
  if(out.what.empty() || out.where.empty()) return false;
  out.unit_path = path;
  return true;
}
//-------------------------------------------------------------------------------------------------

mount_unit* find_mount_unit(mount_db &db, const std::string &mountpoint)
{
  for(auto& mu : db) { if(mu.where == mountpoint) return &mu; } return nullptr;
}
//-------------------------------------------------------------------------------------------------

mount_unit* find_mount_unit(mount_db &db, const mount_info &info)
{
  const string* tie[] = {&info.path, info.dev ? &info.dev->at("LABEL") : 0,
                                     info.dev ? &info.dev->at("UUID")  : 0,
                                     info.dev ? &info.dev->at("PARTLABEL") : 0,
                                     info.dev ? &info.dev->at("PARTUUID")  : 0};
  mount_unit* mu = find_mount_unit(db, info.target);
  return (mu && tie[mu->what_sel] && mu->what == *tie[mu->what_sel]) ? mu : nullptr;
}
//-------------------------------------------------------------------------------------------------

mount_unit* find_mount_unit(mount_db &db, const device_info &dev)
{
  const string* tie[] = {&dev.at("PATH"), &dev.at("LABEL"),  &dev.at("UUID"),
                         &dev.at("PARTLABEL"), &dev.at("PARTUUID")};
  for(auto& mu : db)
    if(tie[mu.what_sel] && mu.what == *tie[mu.what_sel])
      return &mu;
  return nullptr;
}
//-------------------------------------------------------------------------------------------------

bool is_netdev(std::string_view path, std::string_view fstype, std::string_view opt)
{
  size_t pos = fstype.find(",."); //"nfs,nfs4" "fuse.sshfs"
  string_view t = fstype.substr((pos == string_view::npos) ? 0 : pos);
  for(const char* x : {"cifs", "smb3", "nfs", "nfs4",
                       "ncp", "ncpfs", "sshfs", "rclone"})
    if(t == x) return true;
  if(path[0] != '/' && (pos = path.find(':')) != string_view::npos && pos > 0)
    return true;
  if(starts_with(path, "//") || starts_with(path, "\\\\"))
    return true;
  return opt.find("_netdev") != string_view::npos;
}
//-------------------------------------------------------------------------------------------------

static void add_mtp_devices(device_map& out)
{
  static int _init = (LIBMTP_Init(), 1);
  LIBMTP_raw_device_t* rds = nullptr;
  int rds_n = 0;
  switch(LIBMTP_Detect_Raw_Devices(&rds, &rds_n))
   {
    case LIBMTP_ERROR_NONE: break;
    case LIBMTP_ERROR_NO_DEVICE_ATTACHED:                                return;
    case LIBMTP_ERROR_PTP_LAYER:    log("Libmtp: PTP layer error.");     return;
    case LIBMTP_ERROR_USB_LAYER:    log("Libmtp: USB layer error.");     return;
    case LIBMTP_ERROR_MEMORY_ALLOCATION: log("Libmtp: Memory allocation failed."); return;
    case LIBMTP_ERROR_STORAGE_FULL: log("Libmtp: Storage is full.");     return;
    case LIBMTP_ERROR_CONNECTING:   log("Libmtp: Connection error.");    return;
    case LIBMTP_ERROR_CANCELLED:    log("Libmtp: Operation cancelled."); return;
    default:                        log("Libmtp: Unknown error.");       return;
   }
  for(int i = 0; i < rds_n; ++i)
   {
    LIBMTP_mtpdevice_t* dev = LIBMTP_Open_Raw_Device_Uncached(&rds[i]);
    if(!dev) { /*log("Failed to open MTP Device #" + to_string(i + 1));*/ continue; }

    device_info &r = (out.emplace_back(devmap_init_columns()), out.back());

    auto mks = [](char* s){ string t; if(s) { t = s; LIBMTP_FreeMemory(s); } return t; };

    string &name   = r["NAME"]   = mks(LIBMTP_Get_Friendlyname(dev)),
           &model  = r["MODEL"]  = mks(LIBMTP_Get_Modelname(dev)),
           &serial = r["SERIAL"] = mks(LIBMTP_Get_Serialnumber(dev));

    if(name.empty()) name = "MTP#" + to_string(i + 1);
    r["_MTP"]  = to_string(i + 1);
    r["PATH"]  = "/dev/bus/usb/{:03d}/{:03d}" % $(rds[i].bus_location, rds[i].devnum);
    r["_JMTP"] = "{},{}"                      % $(rds[i].bus_location, rds[i].devnum);
    #ifndef AFT_MTP_BEFORE_20230722
      r["_AMTP"] = "{:x}:{:x}" % $(rds[i].device_entry.vendor_id,
                                   rds[i].device_entry.product_id);
    #else
      string &amtp = r["_AMTP"] = serial.empty() ? model : serial;
      auto pred = [](unsigned char c) { return isspace(c); };
      amtp.erase(remove_if(amtp.begin(), amtp.end(), pred), amtp.end());
    #endif
    //TODO: Generate UUID

    uint64_t st_size = 0, st_free = 0;
    for(LIBMTP_devicestorage_t* st = dev->storage; st; st = st->next)
     { st_size += st->MaxCapacity; st_free += st->FreeSpaceInBytes; }

    if(st_size && st_free <= st_size)
     {
      r["SIZE"]    = human_readable_b(st_size, false);
      r["FSAVAIL"] = human_readable_b(st_free, false);
      r["FSUSED"]  = human_readable_b(st_size - st_free, false);
      r["FSUSE%"]  = to_string((st_size - st_free) * 100 / st_size) + "%";
     }
    LIBMTP_Dump_Errorstack(dev);
    LIBMTP_Clear_Errorstack(dev);
    LIBMTP_Release_Device(dev);
   }
  LIBMTP_FreeMemory(rds);
}
//-------------------------------------------------------------------------------------------------
