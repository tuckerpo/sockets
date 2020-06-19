#pragma once
#include <cstdint>
#include <WinSock2.h>

class NetUtils {
 public:
  /**
   * Convert ipv4 address to octal formatted human readable form
   */
  static const char *ip2str_octal(uint32_t ipv4_addr);
};