//
// Created by sigsegv on 3/2/25.
//

#ifndef LIBHTTPTOOLING_NETWSERVER_H
#define LIBHTTPTOOLING_NETWSERVER_H

#include <memory>
#include <mutex>
#include "include/task.h"
#include "Fd.h"

class Poller;

class NetwConnectionHandler {
public:
    virtual ~NetwConnectionHandler() = default;
    virtual size_t AcceptInput(const std::string &) = 0;
    virtual void EndOfConnection() = 0;
};

class NetwServer;

class NetwProtocolHandler {
public:
    virtual NetwConnectionHandler *Create(const std::function<void (const std::string &)> &output, const std::function<void ()> &close) = 0;
    virtual void Release(NetwConnectionHandler *) = 0;
    virtual void SetAssociatedNetwServer(const std::weak_ptr<NetwServer> &) = 0;
};

class NetwConnectionHandlerHandle {
private:
    std::shared_ptr<NetwProtocolHandler> protocol;
    NetwConnectionHandler *handler;
public:
    NetwConnectionHandlerHandle() : protocol(), handler(nullptr) {}
    NetwConnectionHandlerHandle(const std::shared_ptr<NetwProtocolHandler> &protocol, NetwConnectionHandler *handler) : protocol(protocol), handler(handler) {}
    NetwConnectionHandlerHandle(const NetwConnectionHandlerHandle &) = delete;
    NetwConnectionHandlerHandle(NetwConnectionHandlerHandle &&mv) : protocol(std::move(mv.protocol)), handler(mv.handler) {
        mv.handler = nullptr;
    }
    NetwConnectionHandlerHandle &operator = (const NetwConnectionHandlerHandle &) = delete;
    NetwConnectionHandlerHandle &operator = (NetwConnectionHandlerHandle &&mv) {
        if (&mv == this) {
            return *this;
        }
        if (handler != nullptr) {
            protocol->Release(handler);
        }
        protocol = std::move(mv.protocol);
        handler = mv.handler;
        mv.handler = nullptr;
        return *this;
    }
    ~NetwConnectionHandlerHandle() {
        if (handler != nullptr) {
            protocol->Release(handler);
            handler = nullptr;
            protocol = {};
        }
    }
    size_t AcceptInput(const std::string &input);
    void EndOfConnection();
};

struct NetwClient {
    uint64_t id;
    Fd fd;
    std::string inputBuffer;
    std::string outputBuffer;
    NetwConnectionHandlerHandle handle;
    bool closeSocket{false};
};

struct NetwFdOutput {
    uint64_t id{0};
    std::string chunk;
    bool close{false};
};

struct NetwFdOutputStruct {
    std::mutex mtx{};
    std::vector<NetwFdOutput> buffers{};
    bool signaled{false};
};

class NetwServer : public std::enable_shared_from_this<NetwServer> {
private:
    Fd commandInput, commandMonitor, serverSocket;
    std::vector<std::shared_ptr<NetwClient>> clients{};
    std::shared_ptr<NetwProtocolHandler> netwProtocolHandler{};
    std::shared_ptr<NetwFdOutputStruct> outputBuffers{};
    std::string commandBuffer;
    std::vector<std::function<void ()>> acceptReadyCallback{};
    std::vector<std::function<void ()>> commandReadyCallback{};
    std::shared_ptr<Poller> poller{};
    std::mutex mtx{};
    bool quitCommandReceived{false};
    bool quitAccepting{false};
    bool quitPolling{false};
    bool quitLoop{false};
protected:
    NetwServer() = delete;
    NetwServer(int port, const std::shared_ptr<NetwProtocolHandler> &netwProtocolHandler);
    NetwServer(const std::shared_ptr<NetwProtocolHandler> &netwProtocolHandler);
public:
    NetwServer(const NetwServer &) = delete;
    NetwServer(NetwServer &&) = delete;
    NetwServer &operator =(const NetwServer &) = delete;
    NetwServer &operator =(NetwServer &&) = delete;
    static std::shared_ptr<NetwServer> Create(int port, const std::shared_ptr<NetwProtocolHandler> &netwProtocolHandler);
    static std::shared_ptr<NetwServer> Create(const std::shared_ptr<NetwProtocolHandler> &netwProtocolHandler);
private:
    void HandleCommand(Poller &poller, NetwFdOutputStruct &outputBuffers);
    task<void> ConnectionAcceptReady(const std::shared_ptr<NetwServer> &selfptrIn);
    task<void> ConnectionAcceptLoop(const std::shared_ptr<Poller> &poller, const std::shared_ptr<NetwServer> &selfptr);
    task<void> CommandReady(const std::shared_ptr<NetwServer> &selfptrIn);
    task<void> CommandReadLoop(const std::shared_ptr<Poller> &pollerIn, const std::shared_ptr<NetwServer> &selfptr);
    task<void> PollLoop(const std::shared_ptr<NetwServer> &selfptr, const std::shared_ptr<Poller> &pollerInc);
    void AddCommand(Poller &) const;
    void AddServerSocket(Poller &) const;
public:
    int GetCommandFd() const;
    void Connect(const void *ipaddr_norder, size_t ipaddr_len, int port, const std::string &requestData, const std::function<void (NetwConnectionHandler *)> &);
    void Run();
};


#endif //LIBHTTPTOOLING_NETWSERVER_H
