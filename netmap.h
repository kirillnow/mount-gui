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

/** @file netmap.h
 *  @author Kovshov K.A. (kirillnow@gmail.com)
 *  @brief Network mapping.
 */

#ifndef NETMAP_H
#define NETMAP_H
//-------------------------------------------------------------------------------------------------
#include "devmap.h"
#include "wsd_probe.h"
#include "base.h"

device_map network_scan(program_settings& settings, const net_iface_list& ifl);

void update_netdevs_values(device_map& configured, device_map& netscan,
                           const net_iface_list& ifl, const std::string& hostname);

bool netdev_type_eq(std::string_view l, std::string_view r) noexcept;

std::string gethostname();
//-------------------------------------------------------------------------------------------------
#endif // NETMAP_H
