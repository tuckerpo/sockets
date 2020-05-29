#include <gtest/gtest.h>

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#include <WS2tcpip.h>
#include <iphlpapi.h>
#pragma comment(lib, "ws2_32")
#pragma comment(lib, "IPHlpApi")
#define IPV4_ADDRESS_IN_SOCKADDR sin_addr.S_un.S_addr
#define SOCKADDR_LENGTH_TYPE int
#else // !_WIN32
#include <fcntl.h>
#include <netinet/ip.h>
#include <sys/socket.h>
#define IPV4_ADDRESS_IN_SOCKADDR sin_addr.s_addr
#define SOCKADDR_LENGTH_TYPE socklen_t
#define SOCKET int
#define closesocket close
#endif // _WIN32

struct SocketsTests : public ::testing::Test
{

};

TEST_F(SocketsTests, SendMessage) {
    ASSERT_TRUE(true);
}