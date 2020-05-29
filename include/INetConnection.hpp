#pragma once
#include <cstdint>
#include <functional>
#include <vector>

class INetConnection {
public:
    typedef std::function<void(const std::vector<uint8_t>& message)> OnDataCallback;
    typedef std::function<void(void)> OnBrokenCallback;
    /**
     * Connect to a remote address
     * peerAddr remote address
     * peerPort remote port
     * return true on successful connection established
     */
    virtual bool Connect(uint32_t peerAddr, uint16_t peerPort) = 0;
    /**
     * Start listening and receiving data.
     * dataCallback callback for when data is recv'd
     * brokenCallback callback for when connection ends
     */
    virtual bool BeginProcessing(OnDataCallback dataCallback, OnBrokenCallback brokenCallback) = 0;
    /**
     * Return bound connection
     */
    virtual uint32_t GetBoundAddress() const = 0;
    /**
     * Return bound port
     */
    virtual uint16_t GetBoundPort() const = 0;
    /**
     * Get peer's IPV4 address
     */
    virtual uint32_t GetPeerAddress() const = 0;
    /**
     * Get peer's port
     */
    virtual uint16_t GetPeerPort() const = 0;
    /**
     * return true if this object is currently connected to a remote
     */
    virtual bool IsConnected() const = 0;
    /**
     * return true if this object disconnected from remote or there was no remote to d/c from
     * return false on error
     */
    virtual bool Disconnect() const = 0;
    /**
     * Post a vector of bytes to be written.
     * return true if the message was queued to be written, false otherwise.
     */
    virtual bool Write(const std::vector<uint8_t>&) = 0;
};