#include "NetConnection.hpp"
#include "NetConnectionWin32.hpp"
#include <algorithm>

static constexpr std::size_t MAX_READ  = 1 << 16;
static constexpr std::size_t MAX_WRITE = 1 << 16;

NetConnection::NetConnection()
    : m_pPlatform(new NetConnection::Platform())
{
    fprintf(stderr, "NetConnection ctor called\n");
    WSAData wsadata;
    if (!WSAStartup(MAKEWORD(2, 0), &wsadata))
        m_pPlatform->init = true;
}

NetConnection::~NetConnection()
{
    if (m_pPlatform->thread.joinable())
        m_pPlatform->thread.join();
    if (m_pPlatform->init)
        WSACleanup();
    delete m_pPlatform;
    m_pPlatform = nullptr;
}

void NetConnection::NetRoutine()
{
    std::vector<uint8_t> rxBuf = {};
    std::unique_lock<decltype(m_pPlatform->mutex)> NetRoutineLock(m_pPlatform->mutex);
    while (m_pPlatform->sock != INVALID_SOCKET) {
        /* Recv. */
        rxBuf.resize(MAX_READ);
        const int numBytes = ::recv(m_pPlatform->sock, (char *)&rxBuf[0], (int)rxBuf.size(), 0);
        if (numBytes == SOCKET_ERROR) {
            NetRoutineLock.unlock();
            m_onBrokenCallback();
            NetRoutineLock.lock();
            return;
        } else if (numBytes > 0) {
            rxBuf.resize(numBytes);
            NetRoutineLock.unlock();
            m_dataCallback(rxBuf);
            NetRoutineLock.lock();
        } else {
            NetRoutineLock.unlock();
            m_onBrokenCallback();
            NetRoutineLock.lock();
        }
        if (m_pPlatform->sock == INVALID_SOCKET)
            break;
        /* Send. */
        const std::size_t outQueueSize = m_pPlatform->sendQueue.size();
        if (outQueueSize > 0) {
            const std::size_t bufSize  = m_pPlatform->sendQueue.front().size();
            const int sendSize = static_cast<int>(std::min(MAX_WRITE, bufSize));
            std::vector<uint8_t> sendBuffer = m_pPlatform->sendQueue.front();
            m_pPlatform->sendQueue.pop_front();
            const int bytesSent = ::send(m_pPlatform->sock, (const char *)&sendBuffer[0], sendSize, 0);
            fprintf(stderr, "Sent %d bytes\n", bytesSent);
            if (bytesSent == SOCKET_ERROR) {
                const int wsaError = WSAGetLastError();
                if (wsaError != WSAEWOULDBLOCK) {
                    NetRoutineLock.unlock();
                    m_onBrokenCallback();
                    NetRoutineLock.lock();
                }
            } else if (bytesSent > 0) {
                fprintf(stderr, "Wrote some bytes\n");
                // nop
            }
        }

    }
}

bool NetConnection::Connect(uint32_t peerAddr, uint16_t peerPort)
{
    struct sockaddr_in socketAddr;
    ::memset(&socketAddr, 0, sizeof(socketAddr));
    socketAddr.sin_family = AF_INET;
    m_pPlatform->sock = ::socket(socketAddr.sin_family, SOCK_STREAM, 0);
    if (m_pPlatform->sock == INVALID_SOCKET) {
        fprintf(stderr, "NetConnection: Error creating socket\n");
        return false;
    }
    LINGER linger;
    linger.l_onoff = 1;
    linger.l_linger = 0;
    (void)setsockopt(m_pPlatform->sock, SOL_SOCKET, SO_LINGER, (const char*)&linger, sizeof(linger));
    if (::bind(m_pPlatform->sock, (struct sockaddr*)&socketAddr, sizeof(socketAddr)) != 0) {
        fprintf(stderr, "NetConnection: Error binding socket\n");
        return false;
    }
    ::memset(&socketAddr, 0, sizeof(socketAddr));
    socketAddr.sin_family           = AF_INET;
    socketAddr.sin_addr.S_un.S_addr = ::htonl(peerAddr);
    socketAddr.sin_port             = ::htons(peerPort);
    if (::connect(m_pPlatform->sock, (const sockaddr*)&socketAddr, sizeof(socketAddr)) != 0) {
        fprintf(stderr, "NetConnection: Error connecting socket. WSA Error: %d\n", WSAGetLastError());
        return false;
    }
    int sockAddrLen = sizeof(socketAddr);
    if (::getsockname(m_pPlatform->sock, (struct sockaddr*)&socketAddr, &sockAddrLen) == 0) {
        this->m_boundAddr = ::ntohl(socketAddr.sin_addr.S_un.S_addr);
        this->m_boundPort = ::ntohl(socketAddr.sin_port);
    }
    /* We made it this far without erroring, the peer address and port are valid. */
    this->m_peerAddr = peerAddr;
    this->m_peerPort = peerPort;
    return true;
}

bool NetConnection::BeginProcessing(OnDataCallback dataCallback, OnBrokenCallback brokenCallback)
{
    bool bResult = true;
    if (m_pPlatform->sock == INVALID_SOCKET) {
        bResult = false;
        return bResult;
    }
    /* Processing thread is already running. */
    if (m_pPlatform->thread.joinable())
        return bResult;
    m_dataCallback      = dataCallback;
    m_onBrokenCallback  = brokenCallback;
    m_pPlatform->thread = std::thread([this]() {
        this->NetRoutine();
    });
    return bResult;
}

bool NetConnection::Write(const std::vector<uint8_t>& bytes)
{
    m_pPlatform->sendQueue.push_back(bytes);
    return true;
}

bool NetConnection::IsConnected() const
{
    return m_pPlatform->sock != INVALID_SOCKET;
}

uint32_t NetConnection::GetPeerAddress() const
{
    uint32_t addr = 0;
    if (m_pPlatform->sock != INVALID_SOCKET)
        addr = m_peerAddr;
    return addr;
}

uint16_t NetConnection::GetPeerPort() const
{
    return m_peerPort;
}

uint32_t NetConnection::GetBoundAddress() const
{
    return m_boundAddr;
}

uint16_t NetConnection::GetBoundPort() const
{
    return m_boundPort;
}

uint32_t NetConnection::GetAddressFromHost(const std::string& host)
{
    bool wsaStarted = false;
    const std::unique_ptr< WSADATA, std::function< void(WSADATA*) > > wsaData(
        new WSADATA,
        [&wsaStarted](WSADATA* p){
            if (wsaStarted) {
                (void)WSACleanup();
            }
            delete p;
        }
    );
    wsaStarted = !WSAStartup(MAKEWORD(2, 0), wsaData.get());
    struct addrinfo hints;
    (void)memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    struct addrinfo* rawResults;
    if (getaddrinfo(host.c_str(), NULL, &hints, &rawResults) != 0) {
        return 0;
    }
    std::unique_ptr< struct addrinfo, std::function< void(struct addrinfo*) > > results(
        rawResults,
        [](struct addrinfo* p){
            freeaddrinfo(p);
        }
    );
    if (results == NULL) {
        return 0;
    } else {
        struct sockaddr_in* ipAddress = (struct sockaddr_in*)results->ai_addr;
        return ntohl(ipAddress->sin_addr.S_un.S_addr);
    }
}

bool NetConnection::Disconnect() const
{
    bool bResult = true;
    return bResult;
}