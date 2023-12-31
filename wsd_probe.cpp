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

/** @file wsd_probe.cpp
 *  @author Kovshov K.A. (kirillnow@gmail.com)
 *  @brief Simple Window Service Discovery client.
 */
//-------------------------------------------------------------------------------------------------

#include "wsd_probe.h"
#include "common/hires_timer.h"
#include "common/regex.h"
#include <stdexcept>
#include <cctype>
#include <string.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <net/if.h>
#include <poll.h>

//forward declarations; project code should contain those
void log(std::string s, const char* color);
void refresh_ui();
std::string generate_uuid_v1();

#define s_errno() string(strerror(errno))

using namespace std;
//-------------------------------------------------------------------------------------------------

static in_addr inet_aton(const char* addr)
{ if(in_addr r; inet_aton(addr, &r)) return r; throw runtime_error("Invalid IP: "s + addr); }

static in6_addr inet_pton(const char* addr)
{
  if(in6_addr r; inet_pton(AF_INET6, addr, &r)) return r;
  throw runtime_error("Invalid IP: "s + addr);
}
static sockaddr_in make_addr(const string& ip, uint16_t port)
{ return sockaddr_in{AF_INET, htons(port), inet_aton(ip.c_str())}; }

static sockaddr_in6 make_addr6(const string& ip, uint16_t port)
{ return sockaddr_in6{AF_INET6, htons(port), 0, inet_pton(ip.c_str())}; }

static std::string inet_ntop(const in6_addr& src)
{
  char b[INET6_ADDRSTRLEN+1]{};
  if(inet_ntop(AF_INET6, &src, b, (sizeof b) - 1)) return b;
  throw runtime_error("inet_ntop");
}
//-------------------------------------------------------------------------------------------------
static bool contains(const std::string& ip, const wsd_dev_id_list& list) noexcept
{
  for(const auto& x : list) { if(x.ip == ip) return true; } return false;
}
//-------------------------------------------------------------------------------------------------
struct wsd_socket
{
  int fd = -1;
  bool v6 = false;

  wsd_socket() = default;
  wsd_socket(wsd_socket&& r) :fd{r.fd}, v6{r.v6} { r.fd = -1; }

  ~wsd_socket() { if(fd >= 0  && close(fd)) log("close(): " + s_errno(), "red"); }

  bool setup(const net_interface& net)
  {
    v6 = !net.ip4;
    const int one = 1, if_idx = net.idx;

    if((fd = socket(v6 ? AF_INET6 : AF_INET, SOCK_DGRAM|SOCK_CLOEXEC, IPPROTO_UDP)) < 0)
     {log("socket(): " + s_errno(), "red"); return false; }

    if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one))
     { log("setsockopt(SO_REUSEADDR): " + s_errno(), "red"); return false; }

    if(v6 && setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &one, sizeof one))
     { log("setsockopt(IPV6_V6ONLY): " + s_errno(), "red");  return false; }

    sockaddr_in  a4 {AF_INET,  htons(3702), INADDR_ANY};
    sockaddr_in6 a6 {AF_INET6, htons(3702), 0, in6addr_any};

    auto bnd = [&]{ return bind(fd, v6 ? (sockaddr*)&a6 : (sockaddr*)&a4,
                                    v6 ? sizeof a6 : sizeof a4);         };

    if(bnd() && (a4.sin_port = 0, a6.sin6_port = 0, bnd()))
     { log("bind(): " + s_errno(), "red"); return false; }

    static const in_addr  mc_addr4 = inet_aton("239.255.255.250");
    static const in6_addr mc_addr6 = inet_pton("FF02::C");
    if(v6)
     {
      ipv6_mreq mreq{mc_addr6, net.idx};
      if(setsockopt(fd, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP, &mreq, sizeof mreq))
       { log("setsockopt(IPV6_ADD_MEMBERSHIP): " + s_errno(), "red"); return false; }

      if(setsockopt(fd, IPPROTO_IPV6, IPV6_MULTICAST_IF, &if_idx, sizeof if_idx))
       { log("setsockopt(IPV6_MULTICAST_IF): " + s_errno(), "red");   return false; }

      if(setsockopt(fd, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, &one, sizeof one))
       { log("setsockopt(IPV6_MULTICAST_HOPS): " + s_errno(), "red"); return false; }
     }
    else
     {
      ip_mreqn mreq{mc_addr4, in_addr{net.ip4}, if_idx};
      if(setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof mreq))
       { log("setsockopt(IP_ADD_MEMBERSHIP): " + s_errno(), "red");   return false; }

      if(setsockopt(fd, IPPROTO_IP, IP_MULTICAST_IF, &mreq, sizeof mreq))
       { log("setsockopt(IP_MULTICAST_IF): " + s_errno(), "red");     return false; }

      if(setsockopt(fd, IPPROTO_IP, IP_MULTICAST_TTL, &one, sizeof one))
       { log("setsockopt(IP_MULTICAST_TTL): " + s_errno(), "red");    return false; }
     }
    return true;
  }
  bool send(const std::string& data)
  {
    static const sockaddr_in  a4 = make_addr("239.255.255.250", 3702);
    static const sockaddr_in6 a6 = make_addr6("FF02::C",        3702);
    while(-1 == sendto(fd, data.c_str(), data.size(), 0,
                       v6 ? (sockaddr*)&a6 : (sockaddr*)&a4,
                       v6 ? sizeof a6 : sizeof a4))
      if(errno != EINTR) { log("sendto(IPv4): " + s_errno(), "red"); return false; }
    return true;
  }
  bool receive(std::string& data, std::string& src_ip)
  {
    sockaddr_in  a4{};
    sockaddr_in6 a6{};
    socklen_t    al = (v6 ? sizeof a6 : sizeof a4);
    int sz;
    if((sz = recvfrom(fd, data.data(), data.size(), MSG_DONTWAIT,
                      v6 ? (sockaddr*)&a6 : (sockaddr*)&a4, &al)) <= 0)
     {
      if(sz == 0 || errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)
       { src_ip.clear(); return true; }
      log("recvfrom(): " + s_errno(), "red"); return false;
     }
    if(al != (v6 ? sizeof a6 : sizeof a4))
     { log("recvfrom(): truncated address", "red"); return false; }

    src_ip = v6 ? inet_ntop(a6.sin6_addr) : inet_ntoa(a4.sin_addr);
    data.resize(sz);
    return true;
  }
};

//-------------------------------------------------------------------------------------------------
static std::string probe_xml()
{
  return  "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
          "<soap:Envelope xmlns:pnpx=\"http://schemas.microsoft.com/windows/pnpx/2005/10\""
          " xmlns:pub=\"http://schemas.microsoft.com/windows/pub/2005/07\""
          " xmlns:soap=\"http://www.w3.org/2003/05/soap-envelope\""
          " xmlns:wsa=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\""
          " xmlns:wsd=\"http://schemas.xmlsoap.org/ws/2005/04/discovery\""
          " xmlns:wsdp=\"http://schemas.xmlsoap.org/ws/2006/02/devprof\""
          " xmlns:wsx=\"http://schemas.xmlsoap.org/ws/2004/09/mex\">"
          "<soap:Header><wsa:To>urn:schemas-xmlsoap-org:ws:2005:04:discovery</wsa:To>"
          "<wsa:Action>http://schemas.xmlsoap.org/ws/2005/04/discovery/Probe</wsa:Action>"
          "<wsa:MessageID>urn:uuid:" + generate_uuid_v1() + "</wsa:MessageID></soap:Header>"
          "<soap:Body><wsd:Probe><wsd:Types>wsdp:Device</wsd:Types></wsd:Probe>"
          "</soap:Body></soap:Envelope>";
}
//-------------------------------------------------------------------------------------------------

static wsd_dev_id wsd_parse_response(const std::string& ip, const std::string& data)
{
  wsd_dev_id r = {ip};
  string t; smatch m;
  if(regex_search(data, m, "<\\w*:Address[^<]+?urn:uuid:([^<\\s]+)"_re))
    r.uuid = m[1];
  if(regex_search(data, m, "<\\w*:XAddrs[^>]*>\\s*([^<\\s]+)"_re))
    r.xaddr = m[1];
  if(regex_search(data, m, "<\\w*:Types[^>]*>([^<]+)"_re))
    r.types = m[1];
  for(auto& m : re_iter(r.types, ":(?!Device)(\\S+)"_re))
   { if(t.size()) { t.push_back(' '); } t.append(m[1]); }
  r.types = t.size() ? std::move(t) : "Device"s;
  return r;
}
//-------------------------------------------------------------------------------------------------

bool wsd_probe(const net_iface_list& if_list, wsd_dev_id_list& out, bool log_out,
               int probe_wait_ms, int probe_repeats, int total_probes, int refresh_interval)
{
  vector<wsd_socket> sockets; sockets.reserve(if_list.size());
  vector<pollfd>     polls;   polls.reserve(if_list.size());
  for(auto& x : if_list)
   {
    if(sockets.emplace_back(), !sockets.back().setup(x)) return false;
    polls.emplace_back(pollfd{sockets.back().fd, POLLIN, 0});
   }

  string data, probe, src_ip;
  for(int sz, ms, rn = 0, n = 0; n < total_probes; ++n)
   {
    if(n % probe_repeats == 0) { if(log_out) log("WS-Discovery: Sending probe #" +
                                                 to_string(n/probe_repeats+1) + "...", "");
                                 probe = probe_xml();
                               }
    for(auto& s : sockets)
      if(!s.send(probe)) return false;

    for(hires_timer tmr; (ms = tmr.milliseconds()) < probe_wait_ms;)
     {
      if(rn != ms / refresh_interval) { refresh_ui(); rn = ms / refresh_interval; }

      if((sz = poll(polls.data(), polls.size(), refresh_interval)) < 0 &&
         errno != EINTR && errno != EAGAIN)
       { log("poll(): " + s_errno(), "red"); return false; }
      if(sz <= 0) continue;

      for(int i = 0; i < sockets.size(); ++i)
       {
        if(polls[i].revents & POLLERR)
         { log("WSD: Recieved POLLERR.", "red"); return false; }
        if(!(polls[i].revents & POLLIN)) continue;

        if(!sockets[i].receive((data.resize(4096), data), src_ip))
          return false;
        if(src_ip.empty() || contains(src_ip, out))   continue;
        if(data.find(":ProbeMatch>") == string::npos) continue;

        out.emplace_back(wsd_parse_response(src_ip, data));

        if(log_out) log("Recieved response from " + src_ip + " (Types: " +
                        out.back().types + "; XAddr: " + out.back().xaddr + ")", "");
       }
     }
   }
  return true;
}
//-------------------------------------------------------------------------------------------------

bool get_host_name(const std::string& ip, bool ipv6, std::string& out)
{
  union { sockaddr_in6 v6; sockaddr_in v4; } a;
  if(ipv6) a.v6 = {AF_INET6}; else a.v4 = {AF_INET};
  if(!inet_pton(a.v4.sin_family, ip.c_str(), ipv6 ? (void*)&a.v6.sin6_addr : &a.v4.sin_addr))
    return false;
  char buff[NI_MAXHOST+1]{};
  int e = getnameinfo((sockaddr*)&a, ipv6 ? sizeof a.v6 : sizeof a.v4,
                      buff, NI_MAXHOST, 0, 0, NI_NAMEREQD|NI_NOFQDN|NI_IDN);
  if(e && e != EAI_NONAME)
   { log("getnameinfo(" + ip + "): " + gai_strerror(e), "red"); return false; }
  out = e ? "" : buff;
  return true;
}
//-------------------------------------------------------------------------------------------------

std::string get_host_addr(const std::string& host)
{
  static const addrinfo hints{AI_ADDRCONFIG|AI_IDN, AF_UNSPEC, SOCK_STREAM, IPPROTO_TCP};
  addrinfo* r;
  if(int e = getaddrinfo(host.c_str(), 0, &hints, &r)) switch(e)
   {
    default: log("getaddrinfo(" + host + "): " + gai_strerror(e), "red");
    case EAI_NONAME: case EAI_NODATA: case EAI_AGAIN: case EAI_FAIL: case EAI_ADDRFAMILY:
      return {};
   }
  struct _S{ addrinfo* r; ~_S(){ freeaddrinfo(r); } } _s{r};
  return (r->ai_family == AF_INET) ? inet_ntoa(((sockaddr_in*)r->ai_addr)->sin_addr) :
                                     inet_ntop(((sockaddr_in6*)r->ai_addr)->sin6_addr);
}
//-------------------------------------------------------------------------------------------------

net_iface_list list_active_interfaces()
{
  net_iface_list res;
  struct ifaddrs* lst;
  if(getifaddrs(&lst)) { log("getifaddrs(): " + s_errno(), "red"); return {}; }
  struct _S{ ifaddrs* l; ~_S(){ freeifaddrs(l); } } _s{lst};

  struct if_nameindex* idxs;
  if(!(idxs = if_nameindex())) { log("if_nameindex(): " + s_errno(), "red"); return {}; }
  struct _S2{ struct if_nameindex* l; ~_S2(){ if_freenameindex(l); } } _s2{idxs};

  const unsigned rqflags = IFF_UP|IFF_RUNNING|IFF_MULTICAST;
  for(ifaddrs* x = lst; x; x = x->ifa_next)
   {
    if(!x->ifa_addr || (x->ifa_flags & rqflags) != rqflags) continue;

    net_interface r{x->ifa_name};
    bool skip = false;

    for(auto* y = idxs; y->if_name; ++y) if(r.name == y->if_name)
      { r.idx = y->if_index; break; }

    if(x->ifa_addr->sa_family == AF_INET)
     {
      r.ip = inet_ntoa(((sockaddr_in*)x->ifa_addr)->sin_addr);
      r.ip4 = ((sockaddr_in*)x->ifa_addr)->sin_addr.s_addr;
     }
    else if(x->ifa_addr->sa_family == AF_INET6)
     {
      //Skip if interface has an IPv4 address.
      for(ifaddrs* y = lst; y; y = y->ifa_next)
        if(y->ifa_addr && y->ifa_addr->sa_family == AF_INET && r.name == y->ifa_name)
          skip = true;
      r.ip = inet_ntop(((sockaddr_in6*)x->ifa_addr)->sin6_addr);
     }
    else continue;
    for(auto& y : res) if(y.idx == r.idx && !y.ip4 == !y.ip4) skip = true;

    if(!skip) res.emplace_back(std::move(r));
   }
  return res; //TODO: add prefix length
}
//-------------------------------------------------------------------------------------------------
