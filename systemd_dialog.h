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

/** @file systemd_dialog.h
 *  @author Kovshov K.A. (kirillnow@gmail.com)
 *  @brief Functionality of "Create Systemd Mount Unit" dialog.
 */

#ifndef SYSTEMD_DIALOG_H
#define SYSTEMD_DIALOG_H
//-------------------------------------------------------------------------------------------------
#include "base.h"
#include "mainwindow.h"
#include <QDialog>
//-------------------------------------------------------------------------------------------------
namespace Ui {
  class SystemdDialog;
}

class SystemdDialog : public QDialog
{
  Q_OBJECT

public:
  explicit SystemdDialog(MainWindow *parent, bool fstab, mount_info& mnt_info,
                         program_settings& settings, user_info& usr);
  ~SystemdDialog();

private slots:
  void RadioSelect();
  void Accept();

private:
  Ui::SystemdDialog *ui;
  mount_info&       mnt_info;
  program_settings& settings;
  user_info&        usr_info;

  enum id_type_t {PATH, LABEL, UUID, PARTLABEL, PARTUUID} id_type = PATH;

  bool fstab_entry, netdev, fsck, nofail, user,
       noauto, automount, rw_only, force_unmount;

  std::string unit_path, mount_timeout, idle_timeout;

  void GatherInfo();
  std::string WhatStr(id_type_t id);
  std::string OptionStr();
  std::pair<bool, std::string> MakeFstab();
  std::string DevPath();
  std::string MakeMountUnit();
  std::string MakeAutomountUnit();
};
//-------------------------------------------------------------------------------------------------
#endif // SYSTEMD_DIALOG_H
