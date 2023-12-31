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

/** @file wsd_probe.h
 *  @author Kovshov K.A. (kirillnow@gmail.com)
 *  @brief Simple Window Service Discovery client.
 */

#ifndef WSD_H
#define WSD_H
//-------------------------------------------------------------------------------------------------
#include <string>
#include <vector>
#include <cstdint>

struct wsd_dev_id { std::string ip, host, uuid, types, xaddr; };

struct net_interface {std::string name, ip; uint32_t idx, ip4; };

using wsd_dev_id_list    = std::vector<wsd_dev_id>;
using net_iface_list = std::vector<net_interface>;

/** @raram probe_repeats Number of probes with the same uuid */
bool wsd_probe(const net_iface_list& if_list, wsd_dev_id_list& out,
               bool log_out = true, int probe_wait_ms = 1000,
               int probe_repeats = 2, int total_probes = 4, int refresh_interval = 100);

bool get_host_name(const std::string& ip, bool ipv6, std::string& out);

std::string get_host_addr(const std::string& host);

net_iface_list list_active_interfaces();
//-------------------------------------------------------------------------------------------------
#endif // WSD_H
