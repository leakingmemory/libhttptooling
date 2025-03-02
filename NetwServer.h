//
// Created by sigsegv on 3/2/25.
//

#ifndef LIBHTTPTOOLING_NETWSERVER_H
#define LIBHTTPTOOLING_NETWSERVER_H

#include <memory>
#include "include/task.h"
#include "Fd.h"

class Poller;

class NetwServer : public std::enable_shared_from_this<NetwServer> {
private:
    Fd commandInput, commandMonitor, serverSocket;
    std::vector<Fd> clients{};
    std::string commandBuffer;
    std::vector<std::function<void ()>> acceptReadyCallback{};
    std::vector<std::function<void ()>> commandReadyCallback{};
    bool quitCommandReceived{false};
    bool quitAccepting{false};
    bool quitPolling{false};
    bool quitLoop{false};
protected:
    NetwServer() = delete;
    NetwServer(int port);
public:
    NetwServer(const NetwServer &) = delete;
    NetwServer(NetwServer &&) = delete;
    NetwServer &operator =(const NetwServer &) = delete;
    NetwServer &operator =(NetwServer &&) = delete;
    static std::shared_ptr<NetwServer> Create(int port);
private:
    void HandleCommand();
    task<void> ConnectionAcceptReady(const std::shared_ptr<NetwServer> &selfptrIn);
    task<void> ConnectionAcceptLoop(const std::shared_ptr<NetwServer> &selfptr);
    task<void> CommandReady(const std::shared_ptr<NetwServer> &selfptrIn);
    task<void> CommandReadLoop(const std::shared_ptr<NetwServer> &selfptr);
    task<void> PollLoop(const std::shared_ptr<NetwServer> &selfptr, const std::shared_ptr<Poller> &pollerInc);
    void AddCommand(Poller &) const;
    void AddServerSocket(Poller &) const;
public:
    int GetCommandFd() const;
    void Run();
};


#endif //LIBHTTPTOOLING_NETWSERVER_H
