#pragma once

#include "INetConnection.hpp"
#include <string>

// Implements IPv4 for INetConnection
class NetConnection : public INetConnection {
 public:
  struct Platform;

 private:
  OnDataCallback m_dataCallback;
  OnBrokenCallback m_onBrokenCallback;
  uint32_t m_peerAddr = 0;
  uint16_t m_peerPort = 0;
  uint32_t m_boundAddr = 0;
  uint16_t m_boundPort = 0;
  // Opaque pointer to platform specific implementations (syscalls, etc)

  Platform* m_pPlatform = nullptr;
  virtual void NetRoutine();

 public:
  NetConnection();
  virtual ~NetConnection();
  NetConnection(const NetConnection&) = delete;
  NetConnection(NetConnection&&) = delete;
  NetConnection& operator=(const NetConnection&) = delete;
  NetConnection& operator=(NetConnection&&) = delete;

  /**
   * Connect to a remote IPV4 address at port peerPort.
   * Return true if the connection was established, false otherwise.
   */
  virtual bool Connect(uint32_t peerAddr, uint16_t peerPort) override;
  /**
   * Spin up a processing thread that will ::send and ::recv data. Calls
   * dataCallback on data. Calls brokenCallback if the connection dies.
   */
  virtual bool BeginProcessing(OnDataCallback dataCallback,
                               OnBrokenCallback brokenCallback) override;
  /**
   * Return the local bound address.
   */
  virtual uint32_t GetBoundAddress() const override;
  /**
   * Return the local bound port.
   */
  virtual uint16_t GetBoundPort() const override;
  /**
   * Return the remote address we are connected to currently, or zero if not
   * connected.
   */
  virtual uint32_t GetPeerAddress() const override;
  /**
   * Return the remote port we are connected to.
   */
  virtual uint16_t GetPeerPort() const override;
  /**
   * return true if this object is currently connected to a remote
   */
  virtual bool IsConnected() const override;
  /**
   * return true if this object disconnected from remote or there was no remote
   * to d/c from return false on error
   */
  virtual bool Disconnect() const override;
  /**
   * Write `bytes`.
   */
  virtual bool Write(const std::vector<uint8_t>& bytes) override;
  /**
   * Convert a hostname to a IPV4 address
   */
  static uint32_t GetAddressFromHost(const std::string& host);
};