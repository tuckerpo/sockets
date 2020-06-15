#pragma once
#include "NetServer.hpp"
#include <vector>
#include <thread>
#include <queue>
#include <mutex>

struct NetServer::Platform {
  struct Packet {
    uint32_t address;
    uint16_t port;
    std::vector<uint8_t> bytes;
  };

  bool init = false;
  bool running = false;
  SOCKET sock = INVALID_SOCKET;
  std::thread processingThread;
  HANDLE socketEvent = nullptr;
  std::mutex processingMutex;
  std::deque<Packet> out_q;
};