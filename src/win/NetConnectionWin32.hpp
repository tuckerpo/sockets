#pragma once
#define NOMINMAX  // don't let windows.h override std::(min|max)
#include <WinSock2.h>
#include <windows.h>
#include <WS2tcpip.h>
#pragma comment(lib, "ws2_32")
#include <thread>
#include <mutex>
#include <deque>
#include "NetConnection.hpp"

struct NetConnection::Platform {
  SOCKET sock = INVALID_SOCKET;
  bool init = false;
  std::mutex mutex;
  std::thread thread;
  std::deque<std::vector<uint8_t>> sendQueue;
  std::deque<std::vector<uint8_t>> recvQueue;
  static std::shared_ptr<NetConnection> MakeConnectionFromExistingSocket(
      SOCKET socket, uint32_t boundAddr, uint16_t boundPort, uint32_t peerAddr,
      uint16_t peerPort);
};