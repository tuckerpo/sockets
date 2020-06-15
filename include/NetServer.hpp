#pragma once
#include <functional>
#include <memory>
#include "NetConnection.hpp"

class NetServer {

  // Opaque
  struct Platform;
  Platform* m_pPlatform = nullptr;

 public:
  typedef std::function<void(NetConnection*)> NewConnectionCallback;
  typedef std::function<void(uint32_t address, uint16_t port,
                             const std::vector<uint8_t>& bytes)>
      PacketRxCallback;
  enum class Mode {
    Datagram,
    Server,
    MulticastSend,
    MulticastReceive,
  };
  NetServer();
  virtual ~NetServer();
  NetServer(const NetServer&) = delete;
  NetServer(NetServer&&) = default;
  NetServer& operator=(const NetServer&) = delete;
  NetServer& operator=(NetServer&&) = default;

  bool Open(NewConnectionCallback newConnCb, PacketRxCallback rxCb, Mode mode,
            uint32_t localAddr, uint32_t groupAddr, uint16_t port);
  void Close(bool reapThread);
  uint16_t GetBoundPort() const;
  void SendPacket(uint32_t address, uint16_t port,
                  const std::vector<uint8_t>& bytes);
  static std::vector<uint32_t> GetLocalInterfaceAddresses();

 private:
  void ProcessingRoutine(NetServer::Mode);
  NewConnectionCallback m_newConnCb;
  PacketRxCallback m_packetRxCb;
};