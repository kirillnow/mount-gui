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

/** @file config.h
 *  @author Kovshov K.A. (kirillnow@gmail.com)
 *  @brief Global constant definitions.
 */

#ifndef CONFIG_H
#define CONFIG_H
//-------------------------------------------------------------------------------------------------

#ifndef SYSTEM_DIRS
  ///List of restricted mountpoint prefixes (always ask password)
  #define SYSTEM_DIRS "/boot", "/efi", "/etc", "/run", "/usr", "/var", "/dev", \
                      "/proc", "/sys", "/bin", "/sbin", "/lib", "/lib64", "/opt", "/root"
#endif
#ifndef DEFAULT_MOUNTP
  #define DEFAULT_MOUNTP "/mnt/%d"
#endif

#ifndef SUDO_PREF
#define SUDO_PREF "/usr/bin/"
#endif
#ifndef APPROVED_SUDO_CMDS
  #define APPROVED_SUDO_CMDS {SUDO_PREF"sudo", "-n"}, {SUDO_PREF"pkexec"}, {SUDO_PREF"lxsudo"}
#endif

#ifndef CONFIG_FILE_PATH
  #define CONFIG_FILE_PATH "/.config/mount-gui/mount-gui.conf"
#endif

#ifndef SYS_PREF
  #define SYS_PREF      "/usr/bin/"
#endif
#ifndef BIN_NMAP
  #define BIN_NMAP      SYS_PREF"nmap"
  #define SHARE_NMAP    "/usr/share/nmap"
#endif
#ifndef BIN_SMBCLIENT
  #define BIN_SMBCLIENT SYS_PREF"smbclient"
#endif
#ifndef BIN_SHOWMOUNT
  #define BIN_SHOWMOUNT SYS_PREF"showmount"
#endif
#ifndef BIN_AVAHIB
  #define BIN_AVAHIB SYS_PREF"avahi-browse"
#endif
#ifndef BIN_AFT_MTP
  #define BIN_AFT_MTP SYS_PREF"aft-mtp-mount"
#endif
#ifndef BIN_SIMPLE_MTPFS
  #define BIN_SIMPLE_MTPFS SYS_PREF"simple-mtpfs"
#endif
#ifndef BIN_JMTPFS
  #define BIN_JMTPFS SYS_PREF"jmtpfs"
#endif
//-------------------------------------------------------------------------------------------------
#endif // CONFIG_H
