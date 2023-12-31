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
//-------------------------------------------------------------------------------------------------

#include "base.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "systemd_dialog.h"
#include "common/glob.h"
#include <QFont>
#include <QTimer>
#include <QDesktopServices>
#include <iostream>
//#ifdef Q_WS_X11
//  #include "common/x11_hacks.h"
//#endif

using namespace std;
//-------------------------------------------------------------------------------------------------
//required by execute()
void refresh_ui() { qApp->sendPostedEvents(); qApp->processEvents(); }
//-------------------------------------------------------------------------------------------------

MainWindow::MainWindow(QWidget *parent)
  : QMainWindow{parent}, ui{new Ui::MainWindow},
    mnt_helper{settings, usr_info, system_db}
{
  ui->setupUi(this);
  ui->MountButton->setDefaultAction(ui->actionMount);
  ui->UnmountButton->setDefaultAction(ui->actionUnmount);
  ui->StatusBar->addWidget(ui->StatusBarLabel, 1); //uic does not do this

  //call OnActionRefresh() on start
  connect(this, SIGNAL(onShow()), this, SLOT(OnActionRefresh()),
          Qt::ConnectionType(Qt::QueuedConnection | Qt::UniqueConnection));
  connect(&fsw, &QFileSystemWatcher::directoryChanged, this, &MainWindow::FSWatch,
          Qt::ConnectionType(Qt::QueuedConnection));
  connect(&fsw, &QFileSystemWatcher::fileChanged, this, &MainWindow::FSWatch,
          Qt::ConnectionType(Qt::QueuedConnection));

  log_fn = [this](string s, const char* c) { Log(std::move(s), c); };

  usr_info.fetch_current();
  settings.load_settings(usr_info.user_home + CONFIG_FILE_PATH, usr_info);
  if(!usr_info.uid) settings.sudo_cmd.clear();

  settings.hostname = settings.use_hostname;
  if(settings.hostname.empty() || settings.hostname == "auto")
    settings.hostname = gethostname();

  net_if_list = list_active_interfaces();

  if(auto list = glob({"/dev/block", "/dev/bus/usb", "/dev/bus/usb/*"}))
    for(char* p : list)
      if(!fsw.addPath(qstr(p))) log("Failed to add '"s + p + "' to filesystem watch.");
}
//-------------------------------------------------------------------------------------------------

MainWindow::~MainWindow()
{
  delete ui;
}
//-------------------------------------------------------------------------------------------------

void MainWindow::showEvent(QShowEvent *ev)
{
  QMainWindow::showEvent(ev);
  if(ui->StatusBar->maximumHeight()>1024)  //restrict status bar to one line
   {
    ui->StatusBar->setMaximumHeight(ui->StatusBar->height());
    //ui->StatusBarLabel->setText("");
   }
  emit onShow();
}
//-------------------------------------------------------------------------------------------------

void MainWindow::closeEvent(QCloseEvent *event)
{
  if(!fsw.files().contains(qstr(settings.config_file))) //external editor isn't still open
    settings.save_settings(usr_info);
  event->accept();
}
//-------------------------------------------------------------------------------------------------

void MainWindow::Log(std::string line, const char* color)
{
  UpdateStatusLabel(line, color);
  if(line.empty() || line.back() != '\n') line += '\n';
  cerr << line;
  if(color && strlen(color))
    line = "<font color=" + string(color) + ">" + line + "</font>";
  log_buffer.append(line += "<br />");
  emit NewLogLine(qstr(line));
}
//-------------------------------------------------------------------------------------------------

void MainWindow::UpdateStatusLabel(const std::string& text, const char* color)
{
  QFontMetrics metric(ui->StatusBarLabel->font());
  int width = ui->StatusBarLabel->width() - metric.horizontalAdvance(" [LOG] ") - 5;
  if(width < 0) width = 0;
  QString l_txt = metric.elidedText(qstr(text), Qt::ElideRight, width);

  if(color && strlen(color))
    l_txt = "<font color=" + QString(color) + ">" + l_txt + "</font>";
  ui->StatusBarLabel->setText("<big>[<a href=\"#\">Log</a>]</big> " + l_txt);
}
//-------------------------------------------------------------------------------------------------

void MainWindow::PopulateFormFields(device_info& dev)
{
  bool   mounted    = !dev["MOUNTPOINT"].empty();
  string mountpoint = mounted ? dev["MOUNTPOINT"] : settings.mountpoint;
  string options    = dev["OPTIONS"];
  string fs_type    = dev["FSTYPE"];
  string path       = dev["PATH"];

  if(!mounted)
   {
    if(!dev["_MTP"].empty())
      fs_type = "fuse." + settings.default_mtpfs;

    //find a more conventional name for the filesystem
    auto alias_iter = settings.aliases.find(fs_type);
    if(alias_iter != settings.aliases.end())
      fs_type = alias_iter->second;

    if(auto p = settings.find_suitable(fs_type, dev["LABEL"], dev["UUID"]))
     {
      if(!p->mountpoint.empty()) mountpoint = p->mountpoint;
      options = p->options;
     }
    else if(!fs_type.empty()) options = settings.default_opts;

    if(mount_unit* mu = find_mount_unit(system_db, dev))
     {
      mountpoint = mu->where;
      options    = mu->options;
      fs_type    = mu->fstype.empty() ? dev["FSTYPE"] : mu->fstype;
     }
   }

  if(ui->MntDeviceEdit->text() != qstr(path) ||
     !(ui->MntOptionsEdit->document()->isModified() ||
       ui->FsTypeEdit->isModified() || ui->MountpointEdit->isModified()))
   {
    ui->MntOptionsEdit->setPlainText(qstr(options));
    ui->MountpointEdit->setText(qstr(mountpoint));
    ui->MntDeviceEdit->setText(qstr(path));
    ui->FsTypeEdit->setText(qstr(fs_type));
   }
  OnMntDeviceChanged(qstr(path)); //call it even if fields didn't change
}
//-------------------------------------------------------------------------------------------------

void MainWindow::PopulateBlkListWidget()
{
  QTreeWidgetItem* select_item = nullptr;
  string curr_dev = stdstr(ui->MntDeviceEdit->text());

  QFontMetrics metric = ui->BlkListWidget->fontMetrics();
  int col_mw = width()/3, sep2mw = width()/5;

  struct insit { QTreeWidgetItem* wi; string name, sh_type; };
  vector<insit> inserted;

  auto contains = [&](auto&& s, auto&& t)
   { for(auto& x : inserted) if(x.name == s && netdev_type_eq(x.sh_type, t)) return true;
     return false;                                                                         };
  auto comm = [](auto&& n, auto&&... s) { return n + ((s.empty() ? ""s : "  " + s) + ...); };

  ui->BlkListWidget->clear();
  update_netdevs_values(main_dev_map, net_dev_map, net_if_list, settings.hostname);

  for(device_map* devmap : {&main_dev_map, &net_dev_map}) for(auto& i: *devmap)
  {
    string &name = i["NAME"], &pkname = i["PKNAME"], fstype = i["FSTYPE"];

    if(starts_with(fstype, "fuse.") && fstype.size() > 5)
      fstype = get<1>(split2v(fstype, '.'));

    const bool netdev = i["_NETDEV"].size();
    if(netdev && contains(pkname + toupper(name), fstype)) continue;

    QTreeWidgetItem *parent = nullptr, *item = new QTreeWidgetItem;
    set_data(item, &i);
    item->setText(0, qstr(name));            item->setText(1, qstr(i["SIZE"]));
    item->setText(2, qstr(fstype));          item->setText(3, qstr(i["LABEL"]));
    item->setText(4, qstr(i["MOUNTPOINT"])); item->setText(5, qstr(i["UUID"]));

    if(pkname.size()) for(auto& x : inserted) { if(x.name == pkname) parent = x.wi; }
    if(netdev && !parent)
     {
      parent = new QTreeWidgetItem;
      parent->setText(0, qstr(pkname));
      parent->setIcon(0, QIcon(":/icons/host.png"));
      set_data(parent, nullptr);
      ui->BlkListWidget->addTopLevelItem(parent);
      parent->setFirstColumnSpanned(true);
      inserted.emplace_back(insit{parent, pkname});
     }
    else if(!parent) inserted.emplace_back(insit{item, name});

    if(netdev) inserted.emplace_back(insit{item, pkname + toupper(name), fstype});
    //item->setIcon(0, QIcon(":/icons/share.png"));}

    if(parent) parent->addChild(item);
    else ui->BlkListWidget->addTopLevelItem(item);

    if(i["MOUNTPOINT"].empty() && (fstype.empty() || i["SIZE"].empty()))
     {
      string fst = fstype.empty() ? "" : "[" + toupper(fstype) + "]";
      auto* sep = (metric.horizontalAdvance(qstr(name)) > sep2mw) ? "  " : "\t ";
      item->setText(0, qstr(comm(name + sep, fst, i["SIZE"], i["MODEL"], i["SERIAL"])));
      item->setFirstColumnSpanned(true);
     }

    if(i["PATH"] == curr_dev) select_item = item;

    if(parent) continue;
    if(starts_with(name, "sr") || starts_with(name, "scd"))
     { item->setIcon(0, QIcon(":/icons/cd.png")); }
    else if(starts_with(name, "nvme"))
     { item->setIcon(0, QIcon(":/icons/nvme.png")); }
    else if(starts_with(name, "loop"))
     { item->setIcon(0, QIcon(":/icons/loop.png")); }
    else if(i["RM"] == "1")
     { item->setIcon(0, QIcon(":/icons/usb-stick.png")); }
    else if(i["RM"] == "USB")
     { item->setIcon(0, QIcon(":/icons/usb-hdd.png")); }
    else if(name == "fd0" || name == "fd1")
     { item->setIcon(0, QIcon(":/icons/fdd.png")); }
    else
     { item->setIcon(0, QIcon(":/icons/hdd.png")); }
  }
  //TODO: Configurable columns, Partlabel, PartUUID, Used and Available

  int pad = metric.horizontalAdvance(" ");
  ui->BlkListWidget->expandAll();
  for(int i=0; i < ui->BlkListWidget->columnCount(); ++i)
   {
    ui->BlkListWidget->resizeColumnToContents(i);
    int w = min(ui->BlkListWidget->columnWidth(i), col_mw);
    ui->BlkListWidget->setColumnWidth(i, w + (i ? pad : pad*2));
   }
  if(select_item) ui->BlkListWidget->setCurrentItem(select_item);
}
//-------------------------------------------------------------------------------------------------

bool MainWindow::GatherMountInfo(mount_info& out)
{
  out.path    = trim(stdstr(ui->MntDeviceEdit->text()));
  out.fs_type = trim(stdstr(ui->FsTypeEdit->text()));
  out.target  = trim(stdstr(ui->MountpointEdit->text()));
  out.options = trim(stdstr(ui->MntOptionsEdit->toPlainText()));

  if(out.path.empty() || out.fs_type.empty() || out.target.empty())
   { log("Only options field is optional!", ""); return false; }

  if(sanitize_name(out.path).empty())
   { log("Device paths without alphanumerical characters are not supported!", ""); return false; }

  out.dev = find_device(main_dev_map, out.path);
  return true;
}
//-------------------------------------------------------------------------------------------------

void MainWindow::OnBlkListSelection(QTreeWidgetItem* curr)
{
  if(device_info* dev; curr && (dev = get_data(curr)))
    PopulateFormFields(*dev);
}
//-------------------------------------------------------------------------------------------------

void MainWindow::OnMntDeviceChanged(const QString &text)
{
  device_info* dev = find_device(main_dev_map, stdstr(text));
  ui->actionMount->setEnabled(text.size() && (!dev || dev->at("MOUNTPOINT").empty()));
  ui->actionUnmount->setEnabled(dev && !dev->at("MOUNTPOINT").empty());
  ui->actionSave_by_FS->setEnabled(dev && !dev->at("FSTYPE").empty());
  ui->actionSave_by_Label->setEnabled(dev && !dev->at("LABEL").empty());
  ui->actionSave_by_UUID->setEnabled(dev && !dev->at("UUID").empty());
  ui->actionMakeFstabEntry->setEnabled(!ui->MountpointEdit->text().isEmpty());
  ui->actionMakeSystemdUnit->setEnabled(!ui->MountpointEdit->text().isEmpty());
}
//-------------------------------------------------------------------------------------------------

void MainWindow::ShowHelp(const QString&)
{
  QTextBrowser* browser = CreateBrowserWindow("Manpages");
  string src = "qrc:/help/", t = trim(stdstr(ui->FsTypeEdit->text()));
  if(starts_with(t, "ext") && t.size() == 4)
    src += "ext4.html#" + t;
  else if(t == "nfs" || t == "nfs4")
    src += "nfs"s + ".html#OPTIONS";
  else if(t == "cifs" || t == "smb3")
    src += "cifs"s + ".html#OPTIONS";
  else if(t == "btrfs")
    src += "btrfs"s + ".html#OPTIONS";
  else if(starts_with(t, "ntfs"))
    src += "ntfs"s + ".html#OPTIONS";
  else if(starts_with(t, "fuse."))
    src += "fuse"s + ".html#OPTIONS";
  else src += "mount.html#" + t;
  browser->setSource(qstr(src));
}
//-------------------------------------------------------------------------------------------------

void MainWindow::ShowLog(const QString& src)
{
  QFont font; font.setFamily("Monospace"); font.setPointSize(12);
  QTextBrowser* browser = CreateBrowserWindow("Log");
  browser->setFont(font); browser->insertHtml(qstr(log_buffer));
  connect(this, SIGNAL(NewLogLine(QString)),
          browser, SLOT(insertHtml(QString)));
}
//-------------------------------------------------------------------------------------------------

void MainWindow::FSWatch(const QString &path)
{
  IF_REENTRY_RETURN();
  string p = stdstr(path);
  //Qt (5.x) uses IN_MODIFY instead of IN_CLOSE_WRITE, which can cause problems
  //Unfortunately where is no easy solution
  if(p == settings.config_file) { settings.load_settings(settings.config_file, usr_info);
                                  fsw.removePath(path); OnSettingsChanged();              }
  if(starts_with(p, "/dev/"))
   {
    static bool acc = true;
    auto rfr = [&]{ acc = true; OnActionRefresh(); };
    if(acc) { acc = false; QTimer::singleShot(500, this, rfr); }
   }
}
//-------------------------------------------------------------------------------------------------

void MainWindow::OnActionRefresh()
{
  IF_REENTRY_RETURN();
//  QApplication::setOverrideCursor(Qt::WaitCursor);
  setCursor(Qt::WaitCursor);
  try { system_db = scan_systemd_units(); }
  catch(...) { log("Error: exception in scan_systemd_units()."); }
  try { main_dev_map = system_scan_devices(system_db); }
  catch(...) { log("Error: exception in system_scan_devices()."); }
  try { PopulateBlkListWidget(); }
  catch(...) { log("Error: exception in PopulateBlkListWidget()."); }
//  QApplication::restoreOverrideCursor();
  unsetCursor();
}
//-------------------------------------------------------------------------------------------------

void MainWindow::OnActionMount()
{
  IF_REENTRY_RETURN();
  mount_info info;
  if(!GatherMountInfo(info)) return;
  setCursor(Qt::WaitCursor);
  bool r = false;
  try { r = mnt_helper.mount(info); }
  catch(...) { log("Error: exception in mount()."); }
  unsetCursor();
  if(r) OnActionRefresh();
}
//-------------------------------------------------------------------------------------------------

void MainWindow::OnActionUnmount()
{
  IF_REENTRY_RETURN();
  mount_info info;
  if(!GatherMountInfo(info)) return;
  setCursor(Qt::WaitCursor);
  bool r = false;
  try { r = mnt_helper.unmount(info); }
  catch(...) { log("Error: exception in unmount()."); }
  unsetCursor();
  if(r) OnActionRefresh();
}
//-------------------------------------------------------------------------------------------------

void MainWindow::OnActionNetworkScan()
{
  IF_REENTRY_RETURN();
  setCursor(Qt::WaitCursor);
  try { net_dev_map = network_scan(settings, net_if_list); }
  catch(...) { log("Error: exception in network_scan()."); }
  try { PopulateBlkListWidget(); }
  catch(...) { log("Error: exception in PopulateBlkListWidget()."); }
  unsetCursor();
}
//-------------------------------------------------------------------------------------------------

void MainWindow::OnActionSave_by_FS()
{
  mount_info info;
  if(!GatherMountInfo(info)) return;
  mountopt_db_entry entry;
  entry.fs_type    = info.fs_type;
  entry.mountpoint = info.target;
  entry.options    = info.options;
  settings.options_db.emplace_back(std::move(entry));

//  if(settings.save_settings(usr_info))
    log("Options for " + info.fs_type + " has been saved.", "green");
}
//-------------------------------------------------------------------------------------------------

void MainWindow::OnActionSave_by_Label()
{
  mount_info info;
  if(!GatherMountInfo(info)) return;
  if(!info.dev || info.dev->at("LABEL").empty())
   { log("Unknown label for device " + info.path + "!"); return; }

  mountopt_db_entry entry;
  entry.fs_type    = info.fs_type;  entry.label   = info.dev->at("LABEL");
  entry.mountpoint = info.target;   entry.options = info.options;

  settings.options_db.emplace_back(std::move(entry));

//  if(settings.save_settings(usr_info))
    log("Options for '" + info.dev->at("LABEL") + "' has been saved.", "green");
}
//-------------------------------------------------------------------------------------------------

void MainWindow::OnActionSave_by_UUID()
{
  mount_info info;
  if(!GatherMountInfo(info)) return;
  if(!info.dev || info.dev->at("UUID").empty())
   { log("Unknown UUID for device " + info.path + "!"); return; }

  mountopt_db_entry entry;
  entry.fs_type    = info.fs_type;  entry.uuid    = info.dev->at("UUID");
  entry.mountpoint = info.target;   entry.options = info.options;

  settings.options_db.emplace_back(std::move(entry));

  //if(settings.save_settings(usr_info))
    log("Options for '" + info.dev->at("UUID") + "' has been saved.", "green");
}
//-------------------------------------------------------------------------------------------------

void MainWindow::OnActionEditPreferences()
{
  privileges_guard priv;
  if(!priv.drop_if_feasible(usr_info))  return;
  if(!settings.save_settings(usr_info)) return;
  if(!QDesktopServices::openUrl(QUrl(qstr(settings.config_file))))
   { log("Failed to open configuration file in external editor!"); return; }
  fsw.addPath(qstr(settings.config_file));
}
//-------------------------------------------------------------------------------------------------

void MainWindow::OnUseSystemdChanged(bool checked)
{
  if(sender() == ui->actionUse_systemctl)
    settings.use_systemctl = checked;
  if(sender() == ui->actionUse_systemd_umount)
    settings.use_systemd_umount = checked;
}
//-------------------------------------------------------------------------------------------------

void MainWindow::OnSettingsChanged()
{
  ui->actionUse_systemctl->setChecked(settings.use_systemctl);
  ui->actionUse_systemd_umount->setChecked(settings.use_systemd_umount);
}
//-------------------------------------------------------------------------------------------------

void MainWindow::OnActionMakeSystemdUnit()
{
  try
   {
    mount_info info;
    if(!GatherMountInfo(info)) return;
    info.target  = mnt_helper.replace_placeholders(info.target,  info);
    info.options = mnt_helper.replace_placeholders(info.options, info);
    bool fstab = (sender() == ui->actionMakeFstabEntry);

    SystemdDialog dialog{this, fstab, info, settings, usr_info};
    if(dialog.exec() == QDialog::Accepted)
      OnActionRefresh();
   }
  catch(...) { log("Exception in Export to Systemd Unit Dialog."); }
}
//-------------------------------------------------------------------------------------------------

QTextBrowser* MainWindow::CreateBrowserWindow(const std::string &title)
{
  QDialog *Dialog = new QDialog(this);
  Dialog->setModal(false);
  Dialog->resize(825, 450); Dialog->setSizeGripEnabled(true);
  QVBoxLayout *Layout = new QVBoxLayout(Dialog);
  Layout->setSpacing(0); Layout->setContentsMargins(0, 0, 0, 0);
  QTextBrowser *Browser = new QTextBrowser(Dialog);
  Browser->setOpenExternalLinks(true);
  Layout->addWidget(Browser); Dialog->setWindowTitle(qstr(title));
  Dialog->setAttribute( Qt::WA_DeleteOnClose );

  Dialog->show(); Dialog->raise(); Dialog->activateWindow();
  /* Qt forces all dialog windows (even ones that created without parent or as QWidget)
     to always stay on top of main app window (at least on X11).
     So we employ dirty X11 hack to hide taskbar entry of normal window instead. */
  /*#ifdef Q_WS_X11
    HideTaskbarEntry(Dialog);
    Dialog->hide(); Dialog->show(); //Task bar entry shows up again if we don't do this
    HideTaskbarEntry(Dialog);
  #endif*/
  return Browser;
}
//-------------------------------------------------------------------------------------------------

/*void MainWindow::changeEvent(QEvent *event)
{
  //this is necessary because of X11 taskbar hack
  if(event->type() == QEvent::WindowStateChange)
   {
    foreach(QWidget *wnd, QApplication::topLevelWidgets())
      if(wnd!=this) wnd->setWindowState(windowState());
   }
}*/
//-------------------------------------------------------------------------------------------------


