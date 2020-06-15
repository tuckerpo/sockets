#include <winsock2.h>
#include <Windows.h>
#include <iphlpapi.h>
#pragma comment(lib, "ws2_32")
#pragma comment(lib, "IPHlpApi")
#undef ERROR
#undef SendMessage
#undef min
#undef max
#include "NetServerWin32.hpp"
#include "NetConnectionWin32.hpp"

constexpr static std::size_t MAX_READ = 1 << 16;

NetServer::NetServer() : m_pPlatform(new Platform()) {
  WSAData wsaData;
  if (!WSAStartup(MAKEWORD(2, 0), &wsaData)) {
    m_pPlatform->init = true;
    m_pPlatform->running = true;
  }
}

NetServer::~NetServer() {
  Close(true);
  if (m_pPlatform->init) WSACleanup();
  m_pPlatform->running = false;
  if (m_pPlatform) {
    delete m_pPlatform;
    m_pPlatform = nullptr;
  }
}

bool NetServer::Open(NewConnectionCallback newConnCb, PacketRxCallback rxCb,
                     Mode mode, uint32_t localAddr, uint32_t groupAddr,
                     uint16_t port) {
  Close(true);
  auto _mode = (mode == NetServer::Mode::Server) ? SOCK_STREAM : SOCK_DGRAM;
  m_pPlatform->sock = ::socket(AF_INET, _mode, 0);
  if (m_pPlatform->sock == INVALID_SOCKET) {
    fprintf(stderr, "NetServerWin32: ::socket failed: %d\n", WSAGetLastError());
    return false;
  }
  if (mode == NetServer::Mode::MulticastSend) {
    struct in_addr multicastIface {};
    multicastIface.S_un.S_addr = ::htonl(localAddr);
    if (setsockopt(m_pPlatform->sock, IPPROTO_IP, IP_MULTICAST_IF,
                   (const char*)&multicastIface,
                   sizeof(multicastIface)) == SOCKET_ERROR) {
      fprintf(stderr, "NetServerWin32: ::setsockopt failed: %d\n",
              WSAGetLastError());
      Close(false);
      return false;
    }
  } else {
    struct sockaddr_in socketAddr {};
    socketAddr.sin_family = AF_INET;
    if (mode == NetServer::Mode::MulticastReceive) {
      BOOL option = true;
      if (setsockopt(m_pPlatform->sock, SOL_SOCKET, SO_REUSEADDR,
                     (const char*)&option, sizeof(option)) == SOCKET_ERROR) {
        fprintf(stderr, "NetServerWin32: ::setsockopt failed: %d\n",
                WSAGetLastError());
        Close(false);
        return false;
      }
      socketAddr.sin_addr.S_un.S_addr = INADDR_ANY;
    } else {
      socketAddr.sin_addr.S_un.S_addr = ::htonl(localAddr);
    }
    socketAddr.sin_port = static_cast<USHORT>(::htonl(port));
    if (bind(m_pPlatform->sock, (struct sockaddr*)&socketAddr,
             sizeof(socketAddr)) != 0) {
      fprintf(stderr, "NetServerWin32: ::bind failed: %d\n", WSAGetLastError());
      Close(false);
      return false;
    }
    if (mode == NetServer::Mode::MulticastReceive) {
      for (const auto& address : this->GetLocalInterfaceAddresses()) {
        struct ip_mreq multicastGroup;
        multicastGroup.imr_multiaddr.S_un.S_addr = ::htonl(groupAddr);
        multicastGroup.imr_interface.S_un.S_addr = ::htonl(localAddr);
        if (setsockopt(m_pPlatform->sock, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                       (const char*)&multicastGroup,
                       sizeof(multicastGroup)) == SOCKET_ERROR) {
          fprintf(stderr, "NetServerWin32: ::setsockopt failed: %d\n",
                  WSAGetLastError());
          Close(false);
          return false;
        }
      }
    } else {
      int sockAddrLen = sizeof(socketAddr);
      if (getsockname(m_pPlatform->sock, (struct sockaddr*)&socketAddr,
                      &sockAddrLen) == 0) {
        port = ::ntohs(socketAddr.sin_port);
      } else {
        fprintf(stderr, "NetServerWin32: ::getsockname failed: %d\n",
                WSAGetLastError());
        Close(false);
        return false;
      }
    }
  }
  // TODO
  if (mode == NetServer::Mode::Server) {
    if (listen(m_pPlatform->sock, SOMAXCONN) != 0) {
      fprintf(stderr, "NetServerWin32: ::listen failed: %d\n",
              WSAGetLastError());
      Close(false);
      return false;
    }
  }
  m_newConnCb = newConnCb;
  m_packetRxCb = rxCb;
  m_pPlatform->processingThread =
      std::thread([this, mode] { this->ProcessingRoutine(mode); });
  return true;
}

void NetServer::ProcessingRoutine(NetServer::Mode mode) {
  std::vector<uint8_t> local_buffer;
  std::unique_lock<decltype(m_pPlatform->processingMutex)> processingLock(
      m_pPlatform->processingMutex);
  while (m_pPlatform->running) {
    local_buffer.resize(MAX_READ);
    struct sockaddr_in peerAddr;
    int peerAddrSize = sizeof(peerAddr);
    if (mode == NetServer::Mode::Server) {
      const SOCKET client_socket = ::accept(
          m_pPlatform->sock, (struct sockaddr*)&peerAddr, &peerAddrSize);
      if (client_socket == INVALID_SOCKET) {
        const auto socket_err = WSAGetLastError();
        if (socket_err != WSAEWOULDBLOCK) {
          fprintf(stderr, "NetServerWin32: ::accept failed: %d\n",
                  WSAGetLastError());
        }
      } else {
        LINGER linger;
        linger.l_onoff = 1;
        linger.l_linger = 0;
        (void)::setsockopt(client_socket, SOL_SOCKET, SO_LINGER,
                           (const char*)&linger, sizeof(linger));
        uint32_t boundAddr{};
        uint16_t boundPort{};
        struct sockaddr_in boundAddress;
        int boundAddressSize = sizeof(boundAddress);
        if (::getsockname(client_socket, (struct sockaddr*)&boundAddress,
                          &boundAddressSize) == 0) {
          boundAddr = ::ntohl(boundAddress.sin_addr.S_un.S_addr);
          boundPort = ::ntohs(boundAddress.sin_port);
        }
        auto connection =
            NetConnection::Platform::MakeConnectionFromExistingSocket(
                client_socket, boundAddr, boundPort,
                ::ntohl(peerAddr.sin_addr.S_un.S_addr),
                ::ntohl(peerAddr.sin_port));

        m_newConnCb(connection.get());
      }
    } else if (mode == NetServer::Mode::Datagram ||
               mode == NetServer::Mode::MulticastReceive) {
      const int amountRecvd = ::recvfrom(
          m_pPlatform->sock, (char*)local_buffer.data(), local_buffer.size(), 0,
          (struct sockaddr*)&peerAddr, &peerAddrSize);
      if (amountRecvd == SOCKET_ERROR) {
        const auto errCode = WSAGetLastError();
        if (errCode != WSAEWOULDBLOCK) {
          fprintf(stderr, "NetServerWin32: ::recvfrom failed: %d\n", errCode);
          Close(false);
          break;
        }
      } else if (amountRecvd > 0) {
        local_buffer.resize(amountRecvd);
        m_packetRxCb(::ntohl(peerAddr.sin_addr.S_un.S_addr),
                     ::ntohl(peerAddr.sin_port), local_buffer);
      }
    }
    if (!m_pPlatform->out_q.empty()) {
      NetServer::Platform::Packet packet = m_pPlatform->out_q.front();
      (void)memset(&peerAddr, 0, sizeof(peerAddr));
      peerAddr.sin_family = AF_INET;
      peerAddr.sin_addr.S_un.S_addr = ::htonl(packet.address);
      peerAddr.sin_port = ::htonl(packet.port);
      const int amountSent = ::sendto(
          m_pPlatform->sock, (const char*)packet.bytes.data(),
          packet.bytes.size(), 0, (const sockaddr*)&peerAddr, sizeof(peerAddr));
      if (amountSent == SOCKET_ERROR) {
        const auto errCode = WSAGetLastError();
        if (errCode != WSAEWOULDBLOCK) {
          fprintf(stderr, "NetServerWin32: ::sendto failed: %d\n", errCode);
          Close(false);
          break;
        }
      } else {
        if (amountSent != packet.bytes.size()) {
          fprintf(stderr,
                  "NetServerWin32: ::sendto truncated packet (%d < %d)\n",
                  amountSent, packet.bytes.size());
        }
        m_pPlatform->out_q.pop_front();
      }
    }
  }
}

void NetServer::Close(bool reapThread) {
  if (reapThread && m_pPlatform->processingThread.joinable()) {
    m_pPlatform->processingThread.join();
    m_pPlatform->out_q.clear();
  }
  if (m_pPlatform->sock != INVALID_SOCKET) {
    (void)::closesocket(m_pPlatform->sock);
    m_pPlatform->sock = INVALID_SOCKET;
  }
}

uint16_t NetServer::GetBoundPort() const { return 0xdead; }
void NetServer::SendPacket(uint32_t address, uint16_t port,
                           const std::vector<uint8_t>& bytes) {
  std::unique_lock<decltype(m_pPlatform->processingMutex)> sendLock(
      m_pPlatform->processingMutex);
  NetServer::Platform::Packet packet;
  packet.address = address;
  packet.port = port;
  packet.bytes = bytes;
  m_pPlatform->out_q.push_back(std::move(packet));
}

std::vector<uint32_t> NetServer::GetLocalInterfaceAddresses() {
  bool wsaInit = false;
  WSAData wsaData;
  if (!WSAStartup(MAKEWORD(2, 0), &wsaData)) {
    wsaInit = true;
  }
  std::vector<uint8_t> local_buffer;
  local_buffer.reserve(15 * 1024);
  ULONG local_buffer_size = static_cast<ULONG>(local_buffer.size());
  ULONG result = ::GetAdaptersAddresses(
      AF_INET, 0, NULL, (PIP_ADAPTER_ADDRESSES)local_buffer.data(),
      &local_buffer_size);
  if (result == ERROR_BUFFER_OVERFLOW) {
    local_buffer.resize(local_buffer_size);
    result = GetAdaptersAddresses(AF_INET, 0, NULL,
                                  (PIP_ADAPTER_ADDRESSES)local_buffer.data(),
                                  &local_buffer_size);
  }
  std::vector<uint32_t> addresses{};
  if (result == ERROR_SUCCESS) {
    for (PIP_ADAPTER_ADDRESSES adapter =
             (PIP_ADAPTER_ADDRESSES)local_buffer.data();
         adapter; adapter = adapter->Next) {
      for (PIP_ADAPTER_UNICAST_ADDRESS unicastAddr =
               adapter->FirstUnicastAddress;
           unicastAddr; unicastAddr = unicastAddr->Next) {
        struct sockaddr_in* ipAddr =
            (struct sockaddr_in*)unicastAddr->Address.lpSockaddr;
        addresses.push_back(::ntohl(ipAddr->sin_addr.S_un.S_addr));
      }
    }
  }
  if (wsaInit) {
    (void)WSACleanup();
  }
  return addresses;
}
