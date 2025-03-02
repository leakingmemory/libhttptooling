//
// Created by sigsegv on 3/2/25.
//

#include "NetwServer.h"
#include "Poller.h"
#include "include/sync_coroutine.h"
#include <iostream>

NetwServer::NetwServer(int port) {
    auto pipefds = Fd::Pipe(true, true);
    commandInput = std::move(std::get<0>(pipefds));
    commandMonitor = std::move(std::get<1>(pipefds));
    serverSocket = Fd::InetSocket();
    serverSocket.BindListen(port);
    serverSocket.Listen(20);
    serverSocket.SetNonblocking();
}

std::shared_ptr<NetwServer> NetwServer::Create(int port) {
    std::shared_ptr<NetwServer> server{new NetwServer(port)};
    return server;
}

void NetwServer::HandleCommand() {
    while (!commandBuffer.empty()) {
        auto ch = commandBuffer[0];
        commandBuffer.erase(0);
        if (ch == 'q') {
            quitCommandReceived = true;
        } else {
            std::cerr << "Invalid internal command: " << ch << "\n";
        }
    }
}

task<void> NetwServer::ConnectionAcceptReady(const std::shared_ptr<NetwServer> &selfptrIn) {
    std::shared_ptr<NetwServer> selfptr{selfptrIn};
    func_task<void> accReadyTask{[selfptr] (const auto &cb) {
        selfptr->acceptReadyCallback.emplace_back(cb);
    }};
    co_await accReadyTask;
    co_return;
}

task<void> NetwServer::ConnectionAcceptLoop(const std::shared_ptr<NetwServer> &selfptrIn) {
    std::shared_ptr<NetwServer> selfptr{selfptrIn};
    while (!selfptr->quitAccepting) {
        std::cout << "Waiting for connections\n";
        co_await ConnectionAcceptReady(selfptr);
        if (selfptr->quitAccepting) {
            break;
        }
        auto clientFd = serverSocket.Accept();
        if (clientFd) {
            std::cout << "Accepted new connection\n";
        } else {
            std::cout << "No new connection\n";
        }
    }
    selfptr->quitPolling = true;
    co_return;
}

task<void> NetwServer::CommandReady(const std::shared_ptr<NetwServer> &selfptrIn) {
    std::shared_ptr<NetwServer> selfptr{selfptrIn};
    func_task<void> cmdReadyTask{[selfptr] (const auto &cb) {
        selfptr->commandReadyCallback.emplace_back(cb);
    }};
    co_await cmdReadyTask;
    co_return;
}

task<void> NetwServer::CommandReadLoop(const std::shared_ptr<NetwServer> &selfptrIn) {
    std::shared_ptr<NetwServer> selfptr{selfptrIn};
    std::string buffer{};
    while (!selfptr->quitCommandReceived) {
        std::cout << "Await command input\n";
        co_await selfptr->CommandReady(selfptr);
        buffer.resize(16);
        try {
            auto len = commandMonitor.Read(buffer);
            if (len > 0) {
                buffer.resize(len);
                commandBuffer.append(buffer);
                selfptr->HandleCommand();
            }
        } catch (std::exception &e) {
            std::cerr << "Internal command interface failure: " << e.what() << "\n";
            selfptr->quitCommandReceived = true;
        }
    }
    selfptr->quitAccepting = true;
    std::vector<std::function <void ()>> cbs{};
    for (const auto &cb : acceptReadyCallback) {
        cbs.emplace_back(cb);
    }
    acceptReadyCallback.clear();
    for (const auto &cb : cbs) {
        cb();
    }
    co_return;
}

task<void> NetwServer::PollLoop(const std::shared_ptr<NetwServer> &selfptrIn, const std::shared_ptr<Poller> &pollerInc) {
    std::shared_ptr<NetwServer> selfptr{selfptrIn};
    std::shared_ptr<Poller> poller{pollerInc};
    while (!quitPolling) {
        auto result = co_await poller->Poll(10000);
        switch (result) {
            case PollerResult::OK: {
                    auto cmdReadyTpl = poller->GetResults(commandMonitor);
                    auto cmdReady = std::get<0>(cmdReadyTpl) || std::get<2>(cmdReadyTpl);
                    if (cmdReady) {
                        std::vector<std::function<void()>> cbs{};
                        for (const auto &cb: commandReadyCallback) {
                            cbs.emplace_back(cb);
                        }
                        commandReadyCallback.clear();
                        for (const auto &cb: cbs) {
                            cb();
                        }
                    }
                    std::vector<std::function<void()>> cbs{};
                    for (const auto &cb: acceptReadyCallback) {
                        cbs.emplace_back(cb);
                    }
                    acceptReadyCallback.clear();
                    for (const auto &cb: cbs) {
                        cb();
                    }
                }
                break;
            case PollerResult::TIMEOUT:
                std::cout << "Timeout\n";
                break;
            case PollerResult ::ERROR:
                std::cout << "Error\n";
                break;
            default:
                std::cout << "Out in the woods\n";
        }
    }
    quitLoop = true;
}

void NetwServer::AddCommand(Poller &poller) const {
    poller.AddFd(commandMonitor, true, false, true);
}

void NetwServer::AddServerSocket(Poller &poller) const {
    poller.AddFd(serverSocket, true, false, true);
}

int NetwServer::GetCommandFd() const {
    return commandInput;
}

void NetwServer::Run() {
    auto poller = Poller::Create();
    AddCommand(*poller);
    AddServerSocket(*poller);
    auto shrptr = shared_from_this();
    FireAndForget<task<void>>([shrptr] () -> task<void> { return shrptr->ConnectionAcceptLoop(shrptr); });
    FireAndForget<task<void>>([shrptr] () -> task<void> { return shrptr->CommandReadLoop(shrptr); });
    FireAndForget<task<void>>([shrptr, poller] () -> task<void> { return shrptr->PollLoop(shrptr, poller); });
    while (!quitLoop) {
        poller->Runner();
    }
}