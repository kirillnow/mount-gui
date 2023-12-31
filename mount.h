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

/** @file mount.h
 *  @author Kovshov K.A. (kirillnow@gmail.com)
 *  @brief Mount-related functionality.
 */

#ifndef MOUNT_H
#define MOUNT_H
//-------------------------------------------------------------------------------------------------
#include "base.h"
#include "devmap.h"

enum trust_level { TLVL_TRUSTED, TLVL_SYSTEMD,  TLVL_ASKPASS, TLVL_REJECT };

trust_level check_mountpoint(std::string_view dir);

//-------------------------------------------------------------------------------------------------
/** Allows only unicode alphanumerical and specific punctuaton characters ('.','-','_').
 *  Replaces any other characters with '_'. Trims punctuation at beginning, end,
 *  and following any other punctuation character.
 *  For example: '\\192.168.1.1\__SHARE-Ё Й__' -> '192.168.1.1_SHARE-Ё_Й'
 */
std::string sanitize_name(std::string_view label);

///Find option in a comma-separated list
bool contains_opt(std::string_view src, std::string_view opt) noexcept;
//-------------------------------------------------------------------------------------------------

class mount_helper
{
    program_settings& settings;
    user_info&        usr_info;
    mount_db&         system_db;
  public:
    mount_helper(program_settings& s, user_info& u, mount_db& db) noexcept
      :settings{s}, usr_info{u}, system_db{db} {}

    /** The following placeholders are recognized:
     * %d - sanitized short device name, e.g. sda1.
     * %u - user name.
     * %g - user group.
     * %t - filesystem type (lowercase).
     * %l - sanitized LABEL.
     * %L - LABEL as-is.
     * %U - UUID as-is.
     */
    std::string replace_placeholders(const std::string &str, const mount_info& info);

    bool create_mountpoint(const std::string &target);

    bool mount(mount_info& info);
    bool unmount(mount_info& info);

};
//-------------------------------------------------------------------------------------------------
#endif // MOUNT_H
