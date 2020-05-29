#include <NetConnection.hpp>
#include <iostream>
#include <chrono>
#include <thread>

static void DataCallback(const std::vector<uint8_t>& buffer)
{
    std::cout << "CB got " << buffer.size() << " bytes from remote!\n";
    for (const auto& byte : buffer)
        printf("%c", byte);
    printf("\n");

}

static void ErrorCallback()
{
    std::cout << "Connection to remote was closed\n";
}

int main()
{
    NetConnection connection;
    const std::string localhost = "localhost";
    const uint32_t remoteAddress = connection.GetAddressFromHost(localhost);
    fprintf(stderr, "Remote address for '%s' is '%d'\n", localhost.c_str(), remoteAddress);
    connection.Connect(remoteAddress, 8000);
    connection.BeginProcessing(DataCallback, ErrorCallback);
    connection.Write({65, 66, 67});
    connection.Write({68, 69, 70});
    connection.Write({71, 72, 73});
    while (true) 
    {
        printf("Main thread, here!\n");
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

}