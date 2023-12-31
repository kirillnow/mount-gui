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

/** @file mainwindow.cpp
 *  @author Kovshov K.A. (kirillnow@gmail.com)
 *  @brief Main window functionality.
 */

#ifndef MAINWINDOW_HQT_INCLUDED
#define MAINWINDOW_HQT_INCLUDED
//-------------------------------------------------------------------------------------------------
#include "base.h"
#include "devmap.h"
#include "netmap.h"
#include "mount.h"

#include <QMainWindow>
#include <QTreeWidget>
#include <QTextBrowser>
#include <QFileSystemWatcher>
//-------------------------------------------------------------------------------------------------
namespace Ui {
  class MainWindow;
}

class MainWindow : public QMainWindow
{
  Q_OBJECT

public:
  explicit MainWindow(QWidget *parent = 0);
  ~MainWindow();

  ///Create a log/help viewer dialog box window.
  QTextBrowser* CreateBrowserWindow(const std::string &title);

private:
  Ui::MainWindow *ui;

  ///All program settings
  program_settings settings;
  ///Information about current user name, id, group and home
  user_info usr_info;
  ///Content of the log window
  std::string log_buffer;
  ///Main list of devices
  device_map main_dev_map;
  ///Results of netscan
  device_map net_dev_map;
  ///Systemd mount units
  mount_db system_db;
  ///List of netwotk interfaces
  net_iface_list net_if_list;

  mount_helper mnt_helper;

  QFileSystemWatcher fsw;

  void UpdateStatusLabel(const std::string &text, const char* color);
  void PopulateBlkListWidget();

  ///Find options for the given device and show them in GUI.
  void PopulateFormFields(device_info& dev);

  bool GatherMountInfo(mount_info& out);

  ///log() implementation
  void Log(std::string line,  const char* color);

  static device_info* get_data(QTreeWidgetItem* item)
  { return (device_info*)item->data(0, Qt::UserRole).value<void*>(); }

  static void set_data(QTreeWidgetItem* item, device_info* data)
  { return item->setData(0, Qt::UserRole, QVariant::fromValue((void*)data)); }

protected:
    virtual void showEvent(QShowEvent *ev);
    virtual void closeEvent(QCloseEvent *event);
    //virtual void changeEvent(QEvent *event);

signals:
    void onShow();
    void NewLogLine(const QString &text);

public slots:
    void ShowHelp(const QString&);
    void ShowLog(const QString& src);
    void FSWatch(const QString& path);
    void OnActionRefresh();
    void OnActionMount();
    void OnActionUnmount();
    void OnActionNetworkScan();
    void OnActionSave_by_FS();
    void OnActionSave_by_Label();
    void OnActionSave_by_UUID();
    void OnActionEditPreferences();
    void OnActionMakeSystemdUnit();
    void OnMntDeviceChanged(const QString& text);
    void OnUseSystemdChanged(bool checked);
    void OnSettingsChanged();
    void OnBlkListSelection(QTreeWidgetItem* curr);
};
//-------------------------------------------------------------------------------------------------
#endif // MAINWINDOW_HQT_INCLUDED
