#include <NetUtils.hpp>

const char *NetUtils::ip2str_octal(uint32_t ipv4_addr) {
  auto correctOrder = ::ntohl(ipv4_addr);
  return ::inet_ntoa(*(struct in_addr *)&correctOrder);
}