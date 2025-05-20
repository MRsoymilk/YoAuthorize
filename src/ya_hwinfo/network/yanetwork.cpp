#include "yanetwork.h"

#include "platform_def.h"
#if defined(YA_WINDOWS)
#include "hwinfooperator.h"
#endif
#if defined(YA_LINUX)
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netpacket/packet.h>
#include <sys/ioctl.h>
#include <unistd.h>
#endif

#include <fstream>
#include <string>
#include <vector>

YaNETWORK::YaNETWORK() { init(); }

YaNETWORK::~YaNETWORK() {}

std::vector<NETWORK> YaNETWORK::getNETWORK() { return m_networks; }

void YaNETWORK::init() {
#if defined(YA_WINDOWS)
  m_networks.clear();

  WmiQuery wmi;
  if (wmi.initialize()) {
    auto names = wmi.query(L"Win32_NetworkAdapter", L"Caption");
    auto macs = wmi.query(L"Win32_NetworkAdapter", L"MACAddress");
    auto ips = wmi.query(L"Win32_NetworkAdapterConfiguration", L"IPAddress");

    size_t count = names.size();
    for (size_t i = 0; i < count; ++i) {
      if (names[i].empty()) {
        continue;
      }
      NETWORK net;
      if (!names[i].empty())
        net.name = std::string(names[i].begin(), names[i].end());
      if (!macs[i].empty())
        net.mac = std::string(macs[i].begin(), macs[i].end());
      if (!ips[i].empty()) {
        // "{\"192.168.1.1\", \"fe80::abcd\"}"
        std::wstring ipRaw = ips[i];
        size_t start = ipRaw.find(L'"');
        size_t end = ipRaw.find(L'"', start + 1);
        if (start != std::wstring::npos && end != std::wstring::npos &&
            end > start)
          net.ipv4 =
              std::string(ipRaw.begin() + start + 1, ipRaw.begin() + end);
      }

      if (!net.name.empty() || !net.mac.empty() || !net.ipv4.empty()) {
        m_networks.push_back(std::move(net));
      }
    }
  }
#endif
#if defined(YA_LINUX)
  m_networks.clear();

  struct ifaddrs* ifaddr = nullptr;
  if (getifaddrs(&ifaddr) == -1) {
    return;
  }

  auto getInterfaceIndex = [](const std::string& name) -> std::string {
    int index = if_nametoindex(name.c_str());
    return (index > 0) ? std::to_string(index) : "";
  };

  auto getMac = [](const std::string& name) -> std::string {
    std::ifstream file("/sys/class/net/" + name + "/address");
    std::string mac;
    if (file.is_open()) {
      std::getline(file, mac);
    }
    return mac;
  };

  auto getIp4 = [](const std::string& iface) -> std::string {
    struct ifaddrs* ifaddr;
    if (getifaddrs(&ifaddr) == -1) return "";

    std::string ip4;
    for (struct ifaddrs* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
      if (ifa->ifa_addr == nullptr) continue;
      if (ifa->ifa_addr->sa_family == AF_INET && iface == ifa->ifa_name) {
        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &((struct sockaddr_in*)ifa->ifa_addr)->sin_addr, ip,
                  sizeof(ip));
        ip4 = ip;
        break;
      }
    }
    freeifaddrs(ifaddr);
    return ip4;
  };

  for (struct ifaddrs* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
    if (ifa->ifa_addr == nullptr || ifa->ifa_addr->sa_family != AF_PACKET)
      continue;

    std::string iface = ifa->ifa_name;

    NETWORK net;
    net.mac = getMac(iface);
    net.ipv4 = getIp4(iface);
    net.name = iface;

    if (!net.mac.empty() || !net.ipv4.empty()) {
      m_networks.push_back(std::move(net));
    }
  }

  freeifaddrs(ifaddr);
#endif
}
