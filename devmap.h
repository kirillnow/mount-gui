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

/** @file devmap.h
 *  @author Kovshov K.A. (kirillnow@gmail.com)
 *  @brief Mapping devices and system configuration.
 */

#ifndef DEVMAP_H
#define DEVMAP_H
//-------------------------------------------------------------------------------------------------
#include <vector>
#include <map>
#include <string>

//-------------------------------------------------------------------------------------------------
struct mount_unit
{
  enum { PATH, LABEL, UUID, PARTLABEL, PARTUUID, INV_SEL };
  std::string unit_path, what, where, fstype, options;
  int what_sel = PATH;
};

using device_info = std::map<std::string, std::string>;
using device_map  = std::vector<device_info>;
using mount_db    = std::vector<mount_unit>;

///Information about device being mounted and target directory.
struct mount_info
{
  std::string target, path, fs_type, options;
  device_info* dev = nullptr;
};
//-------------------------------------------------------------------------------------------------
/// Unescapes \xFF character code sequences
std::string unescape_hex(std::string_view str);

/** @brief Extaract name=value pair from string.
 *  @details Pair delemiter is a space.
 *  Accounts for spaces around "=" and quotes around value.
 */
bool extract_nameval_pair(const std::string& str, size_t &pos,
                          std::string &name, std::string &value);
//-------------------------------------------------------------------------------------------------

device_map system_scan_devices(const mount_db& system_db);

mount_db scan_systemd_units();
bool     read_systemd_unit(const std::string& path, mount_unit& out);

bool is_netdev(std::string_view path, std::string_view fstype, std::string_view opt);

///Find unit by Where=
mount_unit* find_mount_unit(mount_db& db, const std::string& mountpoint);
///Find unit by What= and Where=
mount_unit* find_mount_unit(mount_db& db, const mount_info& info);
///Find unit by What=
mount_unit* find_mount_unit(mount_db& db, const device_info& dev);
//-------------------------------------------------------------------------------------------------
///Find device by path
inline device_info* find_device(device_map& devmap, const std::string& path)
{ for(auto& d : devmap) { if(d.at("PATH") == path) return &d; } return nullptr; }
///Find device in multiple maps
template<class... Ts> inline device_info* find_device(Ts&&... devmaps, const std::string& path)
{ device_info* d; return ((d = find_device(devmaps, path)) || ...), d; }
//-------------------------------------------------------------------------------------------------
#endif // DEVMAP_H
