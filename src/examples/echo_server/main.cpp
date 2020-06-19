#include <NetServer.hpp>
#include <NetUtils.hpp>
#include <iostream>
#include <chrono>
#include <thread>
#include <WinSock2.h>

int main(const int argc, const char** argv) {
  NetServer::NewConnectionCallback newConnCb = [](NetConnection* conn) {
    std::cout << "New connection from "
              << NetUtils::ip2str_octal(conn->GetBoundAddress());
  };
  NetServer::PacketRxCallback packetCb = [](uint32_t address, uint16_t port,
                                            const std::vector<uint8_t>& bytes) {
    std::cout << "Got data from " << NetUtils::ip2str_octal(address) << ":"
              << std::to_string(port) << std::endl;
    for (auto byte : bytes) printf("%2x ", byte);
    printf("%s", "\n");
  };
  NetServer server;
  // TODO: this api kinda sucks. support establishing connections to addresses
  // via char */str
  server.Open(newConnCb, packetCb, NetServer::Mode::Server, 2130706433,
              2130706433, 9191);
  return EXIT_SUCCESS;
}