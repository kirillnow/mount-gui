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

/** @file netmap.cpp
 *  @author Kovshov K.A. (kirillnow@gmail.com)
 *  @brief Network mapping.
 */
//-------------------------------------------------------------------------------------------------

#include "netmap.h"
#include "wsd_probe.h"
#include "common/execute.h"
#include "common/regex.h"
#include "common/vect_op.h"
#include <fstream>
#include <charconv>
#include <algorithm>
#include <climits>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace std;
//-------------------------------------------------------------------------------------------------
device_info devmap_init_columns();

struct nmap_entry { std::string ip, host; bool smb, rpc, nfs; };
struct net_share  { std::string ip, host, srvr, share, comment;
                    enum { SMB, NFS4, NFS } sh_type;          }; //NFS4 before NFS!
//-------------------------------------------------------------------------------------------------

static string unescape_dec(const string& str)
{
  string::size_type prev_pos = 0, pos = 0;
  unsigned char v;
  string res; res.reserve(str.size());
  while( (pos = str.find('\\', prev_pos)) != string::npos )
   {
    if(str.size() - pos < 4) break;
    res.append(str, prev_pos, pos - prev_pos);
    if(from_chars(&str[pos+1], &str[pos+4], v, 10).ec == std::errc{})
      res.push_back((char)v);
    else res.push_back(' ');
    prev_pos = pos + 4;
   }
  res.append(str, prev_pos, str.size() - prev_pos);
  return res;
}
//-------------------------------------------------------------------------------------------------

static pair<vector<string>, bool> nmap_targets(const string& targets, const net_iface_list& ifl)
{
  vector<string> res;
  bool ip6 = false;
  if(targets.empty() || targets == "auto")
    for(auto& x : ifl)
     { if(x.ip4) res.emplace_back(x.ip + "/24"); else ip6 = true; }
  else
    for(auto& m : re_iter(targets, "(ipv6-link-local)|(\\S+)"_re))
     { if(m[2].matched) res.emplace_back(m[2]); else ip6 = true; }
  //TODO: ask confirmation if network is not local

  return {res, ip6 || res.empty()};
}
//-------------------------------------------------------------------------------------------------

static bool scan_nmap(const vector<string>& targets, bool ipv6, vector<nmap_entry> &out)
{
  //using tmp file instead of std redirects to get 'interactive' output (-v --stats-every 10)
  char tmp[] = "/tmp/mount-gui.XXXXXX";
  if(int fd = mkstemp(tmp); fd >= 0) close(fd);
  else { log("Failed to create temporary file: " + s_errno()); return false; }

  struct _S{ char* f; ~_S(){ if(unlink(f)) log("unlink('"s+f+"'): "+s_errno()); }} _s{tmp};

  //--datadir is required to prevent running user scripts as root
  vector<string> params =
    {BIN_NMAP, "--datadir", SHARE_NMAP, "-p", "445,111,2049", "-v", "--open",
     "--script", "smb-protocols", "--stats-every", "10", "--append-output", "-oG", tmp};

  bool r = false;
  if(targets.size()) r |= !execute(params + targets, true, true);
  if(ipv6) r |= !execute(params += {"-6", "--script-args", "newtargets", "--script",
                                    "targets-ipv6-multicast-mld,targets-ipv6-multicast-echo"},
                         true, true);
  if(!r) return false;

  ifstream file{tmp};
  if(!file) { log("Could not open temorary file: " + s_errno()); return false; }
  for(string l; getline(file, l);)
    if(smatch m; regex_match(l, m, "Host:\\s+(\\S+)\\s\\(([^)]*)\\)\\s+Ports:\\s+"
                                   "(?:(\\d+)\\/open\\/|\\d+)\\D+(?:(\\d+)\\/open\\/|\\d+)?"
                                   "\\D+(?:(\\d+)\\/open\\/|\\d+)?.*"_re))
     {
      auto& o = (out.emplace_back(nmap_entry{m[1], m[2]}), out.back());
      for(int i = 3; i < 6; ++i) if(m[i].matched) switch(stoi(m[i]))
       { case 445: o.smb = 1; break; case 111: o.rpc = 1; break; case 2049: o.nfs = 1; }
     }
  return true;
}
//-------------------------------------------------------------------------------------------------

static void scan_shares(const nmap_entry& tgt, bool smb, bool nfs3, vector<net_share>& out)
{
  stringstream s_out; smatch m;
  if(smb && tgt.smb && !execute({BIN_SMBCLIENT, "-NqgL", tgt.ip}, s_out))
    for(string l; getline(s_out, l);)
      if(regex_match(l, m, "\\s*Disk\\|\\s*([^|]+?)\\s*\\|\\s*(.*?)\\s*$"_re))
        out.emplace_back(net_share{tgt.ip, tgt.host, {}, m[1], m[2], net_share::SMB});

  if(tgt.nfs && !tgt.rpc)
   { out.emplace_back(net_share{tgt.ip, tgt.host, {}, "/", {}, net_share::NFS4}); return; }

  if(!nfs3 || !tgt.nfs) return;
  if(!execute({BIN_SHOWMOUNT, "-e", "--no-headers", tgt.ip}, s_out = {}))
    for(string l; getline(s_out, l);)
      if(regex_match(l, m, "(.+)\\s+\\S+\\s*"_re))
        out.emplace_back(net_share{tgt.ip, tgt.host, {}, m[1], {}, net_share::NFS});
}
//-------------------------------------------------------------------------------------------------

static bool avahi_discover(vector<nmap_entry> &n_map, vector<net_share>& shares)
{
  static constexpr auto NFS = net_share::NFS, NFS4 = net_share::NFS4;
  stringstream s_out; smatch m;
  if(!getuid()) execute({"systemctl", "start", "avahi-daemon"});
  if(exists("/run/avahi-daemon/pid")) log("Avahi-Browse: ...", "");
  if(execute({BIN_AVAHIB, "-artkp"}, s_out))
   { log("Make sure avavi daemon is running."); return false; }
  for(string l; getline(s_out, l);)
    if(regex_match(l, m, "=;[^;]+;[^;]+;([^;]+);([^;]+);[^;]+;([^;]+);([^;]+);"
                         "[^;]+;(?:[^;]*?\"path\\s*=\\s*([^\"]+?)\\s*\")?.*"_re))
     {
      if(m[2] == "_smb._tcp")
        n_map.emplace_back(nmap_entry{m[4], m[3], true, false, false});
      else if(m[2] == "_nfs._tcp")
        shares.emplace_back(net_share{m[4], m[3], {}, m[5].matched?unescape_dec(m[5]):"/",
                                      unescape_dec(m[1]), m[5].matched ? NFS : NFS4});
                                      //nfs3 advertising only '/' seems pretty unlikely
     }
  return true;
}
//-------------------------------------------------------------------------------------------------

static bool wsd_discover(const net_iface_list& ifl, vector<nmap_entry> &n_map)
{
  wsd_dev_id_list lst;
  if(!wsd_probe(ifl, lst)) return false;
  for(wsd_dev_id& x : lst)
    get_host_name(x.ip, x.ip.find(':') != string::npos, x.host);
  for(wsd_dev_id& x : lst)
    n_map.emplace_back(nmap_entry{x.ip, x.host, true, false, false});
  return true;
}
//-------------------------------------------------------------------------------------------------
///Replace hostname for all machine-local addresses.
static void replace_local_hostname(const string& ip, string& host,
                                   const net_iface_list& ifl, const string& hostname)
{
  for(auto& i : ifl) if(ip == i.ip || ip == "::1" || starts_with(ip, "127."))
    { host = hostname; break; }
}
//-------------------------------------------------------------------------------------------------

static void collapse_nmap_list(vector<nmap_entry>& lst)
{
  sort(lst.begin(), lst.end(), [](auto&& l, auto&& r)
  { return (l.ip == r.ip) ? l.host.size() && l.host < r.host : l.ip < r.ip; });
  nmap_entry* p = nullptr;
  for(nmap_entry& x : lst)
   {
    if(p && p->ip == x.ip) { p->nfs |= x.nfs; p->rpc |= x.rpc; p->smb |= x.smb; }
    else p = &x;
   }
  auto e = unique(lst.begin(), lst.end(), [](auto&& l, auto&& r){ return l.ip == r.ip; });
  lst.erase(e, lst.end());
}
//-------------------------------------------------------------------------------------------------

static void collapse_share_list(vector<net_share>& lst, const net_iface_list& ifl,
                                const string& hostname)
{
  for(auto& x : lst) replace_local_hostname(x.ip, x.host, ifl, hostname);

  for(auto& x : lst) if(x.host.empty())
    for(auto& y : lst) if(y.host.size() && x.ip == y.ip)
      { x.host = y.host; break; }

  for(auto& x : lst) x.srvr = (x.host.size() ? x.host : x.ip);

  constexpr auto eq = [](auto l, auto r) -> bool
   { return l == r || (l >= net_share::NFS4 && l < r && r <= net_share::NFS); };

  sort(lst.begin(), lst.end(), [&](auto&& l, auto&& r)
       { return l.srvr == r.srvr ? eq(l.sh_type, r.sh_type) ? l.share < r.share :
                l.sh_type < r.sh_type : (l.host.empty() == r.host.empty() ?
                l.srvr < r.srvr : l.host.empty() < r.host.empty()); });
  net_share* p = nullptr;
  for(net_share& x : lst)
   {
    if(p && p->srvr == x.srvr && eq(p->sh_type, x.sh_type) && p->share == x.share)
     { if(p->comment.size() < x.comment.size()) p->comment = x.comment; }
    else p = &x;
   }
  auto e = unique(lst.begin(), lst.end(), [&](auto&& l, auto&& r)
                  { return l.srvr==r.srvr && eq(l.sh_type, r.sh_type) && l.share==r.share; });
  lst.erase(e, lst.end());
}
//-------------------------------------------------------------------------------------------------

device_map network_scan(program_settings& settings, const net_iface_list& ifl)
{
  const bool have_avahi     = exists(BIN_AVAHIB),
             have_smbclient = exists(BIN_SMBCLIENT),
             have_showmount = exists(BIN_SHOWMOUNT),
             have_nmap      = exists(BIN_NMAP);
  //No network interfaces was detected
  if(!have_smbclient)
    log("Smbclient was not found.\nSmbclient is required for finding SMB shares.");
  if(!have_showmount && have_nmap && settings.use_nmap)
    log("Showmount was not found.\n"
        "Showmount (nfs-utils) is required for finding NFSv3 shares with nmap.");
  vector<nmap_entry> n_map;
  vector<net_share> shares;
  if(ifl.empty())
   {
    log("No network interfaces was detected.");
    if(settings.use_wsd) log("Skipping WS-Discovery.", "");
    if(have_nmap && settings.use_nmap)
      log("Switching to IPv6 link-local scanning as fallback for nmap.", "");
   }
  if(have_avahi && settings.use_avahi)
    avahi_discover(n_map, shares);
  if(ifl.size() && settings.use_wsd)
    wsd_discover(ifl, n_map);
  if(have_nmap && settings.use_nmap)
   {
    auto [tgt, ipv6] = nmap_targets(settings.nmap_networks, ifl);
    scan_nmap(tgt, ipv6, n_map);
   }
  collapse_nmap_list(n_map);
  for(auto& x : n_map) scan_shares(x, have_smbclient, have_showmount, shares);
  collapse_share_list(shares, ifl, settings.hostname);
  log("Network scan completed.", "green");
  device_map res;
  for(auto& x : shares)
   {
    auto& d = (res.emplace_back(devmap_init_columns()), res.back());
    if(x.sh_type == net_share::SMB)
     { d["PATH"] = "//"; if(x.share[0] != '/') x.share.insert(0, 1, '/'); }
    else { if(x.share[0] != ':') x.share.insert(0, 1, ':'); }

    const char* tbl[] = {"cifs", "nfs4", "nfs"};
    d["_NETDEV"] = "1";                d["PATH"]  += x.srvr + x.share;
    d["NAME"]    = std::move(x.share); d["HOST"]   = std::move(x.host);
    d["IP"]      = std::move(x.ip);    d["PKNAME"] = std::move(x.srvr);
    d["FSTYPE"]  = tbl[x.sh_type];     d["MODEL"]  = std::move(x.comment);
   }
  return res;
}
//-------------------------------------------------------------------------------------------------

static pair<string,string> get_host_info(const string& addr)
{
  string host;
  bool v4 = true, v6 = false;
  for(char c : addr) { if(c == ':') { v4 = false; v6 = true; break; }
                       if(!isdigit(c) && c != '.') { v4 = false;    } }
  if((v4 || v6) && get_host_name(addr, v6, host))
    return {addr, host};
  return {get_host_addr(addr), addr};
}
//-------------------------------------------------------------------------------------------------

void update_netdevs_values(device_map& configured, device_map& netscan,
                           const net_iface_list& ifl, const std::string& hostname)
{
  for(auto& x : configured)
   {
    if(x["_NETDEV"].empty()) continue;

    string &comment = x["MODEL"],  &ip   = x["IP"],
           &fstype  = x["FSTYPE"], &host = x["HOST"], shr = toupper(x["NAME"]);

    tie(ip, host) = get_host_info(x["PKNAME"]);
    replace_local_hostname(ip, host, ifl, hostname);
    for(auto& y : netscan)
     {
      string &y_ip = y["IP"], &y_host = y["HOST"];
      if(!(ip.size() && y_ip.size() && ip == y_ip) &&
         !(host.size() && y_host.size() && host == y_host))
        continue;

      //hostnames for mounted or preconfigured devices take precedence over resolved ones
      if(host.empty() && y_host.size()) host = y_host;
      else if(host.size() && host != y_host)
        y["PKNAME"] = host; //leave ["HOST"] alone

      if(netdev_type_eq(fstype, y["FSTYPE"]) && shr == toupper(y["NAME"]) &&
         comment.size() < y["MODEL"].size())
        comment = y["MODEL"];
     }
    x["PKNAME"] = host.empty() ? ip : host;
   }
}
//-------------------------------------------------------------------------------------------------

bool netdev_type_eq(std::string_view l, std::string_view r) noexcept
{
  size_t p1 = l.find(",."), p2 = r.find(",."); //"nfs,nfs4" "fuse.sshfs"
  l = l.substr((p1 == string_view::npos) ? 0 : p1);
  r = r.substr((p2 == string_view::npos) ? 0 : p2);
  if(l == r) return true;
  if(((l == "nfs4" || l == "nfs") && (r == "nfs4" || r == "nfs")) ||
     ((l == "cifs" || l == "smb3") && (r == "cifs" || r == "smb3")))
    return true;
  return false;
}
//-------------------------------------------------------------------------------------------------

std::string gethostname()
{
  char b[HOST_NAME_MAX+1]{};
  return gethostname(b, HOST_NAME_MAX) ? "localhost" : b;
}
//-------------------------------------------------------------------------------------------------
/*std::string get_default_ip()
{
  stringstream o; string s; smatch m;
  return (!execute({SYS_PREF"ip", "-4", "-j", "route", "show", "default"}, o) &&
          regex_search(s = o.str(), m, "\"prefsrc\":\"([^\"]+)\""_re)) ? m[1] : ""s;
}*/
//-------------------------------------------------------------------------------------------------
