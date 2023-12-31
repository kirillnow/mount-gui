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

/** @file systemd_dialog.cpp
 *  @author Kovshov K.A. (kirillnow@gmail.com)
 *  @brief Functionality of "Create Systemd Mount Unit" dialog.
 */
//-------------------------------------------------------------------------------------------------

#include "systemd_dialog.h"
#include "ui_systemd_dialog.h"
#include "common/systemd_escape.h"
#include "common/execute.h"
#include "common/regex.h"
#include "common/vect_op.h"
#include <locale>
#include <fstream>
#include <vector>
#include <string>
#include <QFileInfo>
#include <QMessageBox>
#include <QRegularExpressionValidator>

#define MsgBoxErr(title, msg) (QMessageBox::critical(this, qstr(title), qstr(msg)))

using namespace std;
//-------------------------------------------------------------------------------------------------
static string_view read_opt(string_view src, string_view opt)
{
  for(size_t p = 0; (p = src.find(opt, p)) != string_view::npos; p += opt.size())
    if((p == 0 || src[p-1] == ',') && p+opt.size()+1 < src.size() && src[p+opt.size()] == '=')
     {
      size_t p2 = src.find(',', p + opt.size() + 1);
      if(p2 == string_view::npos) p2 = src.size();
      return src.substr(p + opt.size() + 1, p2 - p - opt.size() - 1);
     }
  return {};
}
//-------------------------------------------------------------------------------------------------

static void remove_opt(string& src, string_view opt)
{
  for(size_t p = 0; (p = src.find(opt, p)) != string_view::npos; )
    if((p == 0 || src[p-1] == ',') && (p == src.size()-opt.size() ||
                                       src[p+opt.size()] == ',' || src[p+opt.size()] == '='))
     {
      size_t p2 = src.find(',', p + opt.size());
      if(p2 == string_view::npos) p2 = src.size(); else ++p2;
      src.erase(p, p2 - p);
     }
    else p += opt.size();
  if(src.size() && src.back() == ',') src.pop_back();;
}
//-------------------------------------------------------------------------------------------------

static string fstab_esc(string src)
{
  for(size_t p = 0; (p = src.find_first_of(" \t", p)) != string::npos; )
    src.replace(p, 1, src[p] == ' ' ? "\\040" : "\\011");
  return src;
}
//-------------------------------------------------------------------------------------------------

SystemdDialog::SystemdDialog(MainWindow *parent, bool fstab, mount_info& mnt_info,
                             program_settings& settings, user_info& usr)
                            :QDialog(parent), ui(new Ui::SystemdDialog),
                             mnt_info(mnt_info), settings(settings),
                             fstab_entry(fstab), usr_info(usr)
{
  ui->setupUi(this);
  //We need unicode locale for device label processing
  if(!iswalnum(L'Б'))
   { setlocale(LC_ALL, "en_US.UTF-8"); locale::global(std::locale("en_US.UTF-8")); }

  if(fstab)
   {
    ui->EditFileName->setText("/etc/fstab");
    //ui->EditFileName->setReadOnly(true);
   }
  else ui->EditFileName->setText(qstr("/etc/systemd/system/" +
                                      systemd_escape(mnt_info.target, true)+".mount"));
  ui->CheckUser->setEnabled(fstab);
  ui->CheckForceUnmount->setEnabled(!fstab);
  if(fstab) setWindowTitle("Create an fstab entry");

  netdev = contains_opt(mnt_info.options, "_netdev") ||
           is_netdev(mnt_info.path, mnt_info.fs_type, mnt_info.options);

  ui->CheckFsck->setChecked(!netdev); ui->CheckFsck->setEnabled(!netdev);

  ui->LabelWhere->setText(qstr(mnt_info.target));
  RadioSelect();

  ui->RadioLABEL->setEnabled(mnt_info.dev && mnt_info.dev->at("LABEL").size());
  ui->RadioUUID->setEnabled(mnt_info.dev  && mnt_info.dev->at("UUID").size());
  ui->RadioPARTLABEL->setEnabled(mnt_info.dev && mnt_info.dev->at("PARTLABEL").size());
  ui->RadioPARTUUID->setEnabled(mnt_info.dev  && mnt_info.dev->at("PARTUUID").size());

  ui->CheckAutomount->setChecked(contains_opt(mnt_info.options, "x-systemd.automount"));
  ui->CheckRwOnly->setChecked(contains_opt(mnt_info.options, "x-systemd.rw-only"));
  ui->CheckNoauto->setChecked(contains_opt(mnt_info.options, "noauto"));
  ui->CheckNofail->setChecked(contains_opt(mnt_info.options, "nofail"));
  ui->CheckUser->setChecked(contains_opt(mnt_info.options, "user") && fstab);

  using V = QRegularExpressionValidator; using R = QRegularExpression;
  ui->EditMountTimeout->setText(qstr(read_opt(mnt_info.options, "x-systemd.mount-timeout")));
  ui->EditIdleTimeout->setText(qstr(read_opt(mnt_info.options,  "x-systemd.idle-timeout")));
  ui->EditMountTimeout->setValidator(new V(R("(\\d+[smh]?)*"), ui->EditMountTimeout));
  ui->EditIdleTimeout->setValidator(new V(R("(\\d+[smh]?)*"), ui->EditIdleTimeout));
}
//-------------------------------------------------------------------------------------------------

SystemdDialog::~SystemdDialog()
{
  delete ui;
}
//-------------------------------------------------------------------------------------------------

void SystemdDialog::RadioSelect()
{
  id_type = ui->RadioPATH->isChecked()      ? PATH      :
            ui->RadioLABEL->isChecked()     ? LABEL     :
            ui->RadioUUID->isChecked()      ? UUID      :
            ui->RadioPARTLABEL->isChecked() ? PARTLABEL : PARTUUID;

  ui->LabelWhat->setText(qstr(WhatStr(id_type)));
}
//-------------------------------------------------------------------------------------------------

void SystemdDialog::GatherInfo()
{
  automount = ui->CheckAutomount->isChecked();
  rw_only   = ui->CheckRwOnly->isChecked();
  noauto    = ui->CheckNoauto->isChecked();
  nofail    = ui->CheckNofail->isChecked();
  user      = ui->CheckUser->isChecked();
  fsck      = ui->CheckFsck->isChecked();
  force_unmount = ui->CheckForceUnmount->isChecked();

  mount_timeout = stdstr(ui->EditMountTimeout->text());
  idle_timeout  = stdstr(ui->EditIdleTimeout->text());
  unit_path     = stdstr(ui->EditFileName->text());
}
//-------------------------------------------------------------------------------------------------

string SystemdDialog::WhatStr(id_type_t id)
{
  const char* tbl[] = {{}, "LABEL", "UUID", "PARTLABEL", "PARTUUID"};
  if(id == PATH) return mnt_info.path;
  if(!mnt_info.dev) return {};
  string& w = mnt_info.dev->at(tbl[id]);
  return w.size() ? tbl[id] + "="s + w : w;
}
//-------------------------------------------------------------------------------------------------

string SystemdDialog::OptionStr()
{
  string res = mnt_info.options;
  remove_opt(res, "x-systemd.automount"); if(automount) res += ",x-systemd.automount";
  remove_opt(res, "x-systemd.rw-only");   if(rw_only)   res += ",x-systemd.rw-only";
  remove_opt(res, "_netdev");             if(netdev)    res += ",_netdev";
  remove_opt(res, "noauto");              if(noauto)    res += ",noauto";
  remove_opt(res, "nofail");              if(nofail)    res += ",nofail";
  remove_opt(res, "user");                if(user)      res += ",user";

  remove_opt(res, "x-systemd.mount-timeout");
  if(mount_timeout.size()) res += ",x-systemd.mount-timeout=" + mount_timeout;
  remove_opt(res, "x-systemd.idle-timeout");
  if(idle_timeout.size()) res += ",x-systemd.idle-timeout=" + idle_timeout;

  if(res[0] == ',') res.erase(0, 1);
  return res;
}
//-------------------------------------------------------------------------------------------------

std::pair<bool, string> SystemdDialog::MakeFstab()
{
  string res; bool overwrite = false;
  string wa[] = { WhatStr(PATH), WhatStr(LABEL), WhatStr(UUID),
                  WhatStr(PARTLABEL), WhatStr(PARTUUID)         };

  string entry = fstab_esc(WhatStr(id_type)) + '\t' + fstab_esc(mnt_info.target) + '\t' +
                 mnt_info.fs_type + '\t' + fstab_esc(OptionStr()) + "\t0\t";

  string fsck_pass = fsck ? "3" : "0";

  ifstream fstab{"/etc/fstab"};
  if(!fstab) throw runtime_error("Can't open '/etc/fstab'!");
  for(string line; getline(fstab, line); )
   {
    bool skip = false;
    for(auto& w : wa) if(w.size() && starts_with(line, w))
     {
      if(overwrite) { skip = true; break; } //skip duplicate entries
      overwrite = true;
      if(smatch m; fsck && regex_match(line, m, "(?:\\S+\\s+){5}(\\d+)\\s*"_re))
        if(m.str(1) != "0") fsck_pass = m[1];
      line = entry + fsck_pass;
      break;
     }
    if(!skip) { res += line; res += '\n'; }
   }
  if(!overwrite) { res += entry; res += fsck_pass + '\n'; }
  return {overwrite, res};
}
//-------------------------------------------------------------------------------------------------

string SystemdDialog::DevPath()
{
  const char* tbl[] = {{}, "label", "uuid", "partlabel", "partuuid"};
  if(id_type == PATH) return mnt_info.path;
  if(!mnt_info.dev) return {};
  string& w = mnt_info.dev->at(toupper(tbl[id_type]));
  return "/dev/disk/by-"s + tbl[id_type] + "/" + dev_disk_by_label_escape(w);
}
//-------------------------------------------------------------------------------------------------

string SystemdDialog::MakeMountUnit()
{
  string dev_path = systemd_escape(DevPath(), true),
         res = "[Unit]\nDocumentation=Mount unit for " + mnt_info.target + "\n",
         fs_tgt = (netdev ? "remote-fs" : "local-fs") + ".target"s;

  if(!nofail) res += "Before=" + fs_tgt + '\n';
  if(fsck)    res += "Requires=systemd-fsck@" + dev_path + ".service\n"
                  +  "After=systemd-fsck@" + dev_path + ".service\n";

  if(!netdev && starts_with(mnt_info.path, "/dev/"))
    res += "After=blockdev@" + dev_path + ".target\n";

  res += "\n[Mount]\nWhat=" + WhatStr(id_type)
      +  "\nWhere=" + mnt_info.target + "\nType=" + mnt_info.fs_type
      +  "\nOptions=" + OptionStr() + "\n";

  if(mount_timeout.size()) res += "TimeoutSec=" + mount_timeout + "\n";
  if(force_unmount)        res += "ForceUnmount=yes\n";
  if(rw_only)              res += "ReadWriteOnly=yes\n";

  res += "\n[Install]\n"s + (nofail ? "WantedBy=" : "RequiredBy=") + fs_tgt + '\n';
  return res;
}
//-------------------------------------------------------------------------------------------------

string SystemdDialog::MakeAutomountUnit()
{
  string res = "[Unit]\nDocumentation=Automount unit for " + mnt_info.target
             + "\n\n[Automount]\nWhere=" + mnt_info.target + "\n";

  if(idle_timeout.size()) res += "TimeoutIdleSec=" + idle_timeout + "\n";

  res += "\n[Install]\n"s + (nofail ? "WantedBy=" : "RequiredBy=")
                          + (netdev ? "remote-fs" : "local-fs") + ".target\n";
  return res;
}
//-------------------------------------------------------------------------------------------------

void SystemdDialog::Accept()
{
  string content, automnt_path;
  int mlines = 0, alines = 0;
  GatherInfo();
  if(fstab_entry)
   {
    bool ow; tie(ow, content) = MakeFstab();

    if(ow && QMessageBox::warning(this, qstr("Overwrite entry?"),
        qstr("FSTAB entry for '" + mnt_info.path + "' already exists!\nOverwrite it?"),
        QMessageBox::Yes|QMessageBox::Cancel) != QMessageBox::Yes)
     { reject(); return; }
   }
  else
   {
    if(exists(unit_path) && QMessageBox::warning(this, qstr("Overwrite file?"),
        qstr("Unit file '" + unit_path + "' already exists!\nOverwrite it?"),
        QMessageBox::Yes|QMessageBox::Cancel) != QMessageBox::Yes)
     { reject(); return; }

    content = MakeMountUnit();
    if(automount)
     {
      automnt_path = replace_suffix(unit_path, ".mount", ".automount");
      string unit =  MakeAutomountUnit();
      for(char c : unit) if(c == '\n') ++alines;
      content += unit; mlines -= alines;
     }
   }
  for(char c : content) if(c == '\n') ++mlines;

  string cmd = SYS_PREF"head -qn " + to_string(mlines) +" >'" + unit_path + "'";
  if(automount && !fstab_entry)
    cmd += " && " SYS_PREF"head -qn " + to_string(alines) + " >'" + automnt_path + "'";
  cmd += " && systemctl daemon-reload";
  if(!noauto && !fstab_entry)
    cmd += " && " SYS_PREF"systemctl enable '" + (automount ? automnt_path : unit_path) + "'";

  vector<string> params = settings.sudo_cmd;
  params += {SYS_PREF"sh", "-c", std::move(cmd)};

  privileges_guard priv;
  if(!priv.drop_if_feasible(usr_info))
   { MsgBoxErr("Error!", "Failed to drop superuser privileges!\nSee log for details."); return; }

  if(execute(params, true, false, content))
   { MsgBoxErr("Error!", "Failed to modify system configuration!\nSee log for details."); return; }

  log("System configuration was modified succefully.", "green");
  accept();
}
//-------------------------------------------------------------------------------------------------
