//
// Created by sigsegv on 3/2/25.
//

#include "NetwServer.h"
#include "Poller.h"
#include "include/sync_coroutine.h"
#include <iostream>
extern "C" {
#include <unistd.h>
}

size_t NetwConnectionHandlerHandle::AcceptInput(const std::string &input) {
    return handler->AcceptInput(input);
}

NetwServer::NetwServer(int port, const std::shared_ptr<NetwProtocolHandler> &netwProtocolHandler) : outputBuffers(std::make_shared<NetwFdOutputStruct>()), netwProtocolHandler(netwProtocolHandler) {
    auto pipefds = Fd::Pipe(true, true);
    commandInput = std::move(std::get<0>(pipefds));
    commandMonitor = std::move(std::get<1>(pipefds));
    serverSocket = Fd::InetSocket();
    serverSocket.BindListen(port);
    serverSocket.Listen(20);
    serverSocket.SetNonblocking();
}

std::shared_ptr<NetwServer> NetwServer::Create(int port, const std::shared_ptr<NetwProtocolHandler> &netwProtocolHandler) {
    std::shared_ptr<NetwServer> server{new NetwServer(port, netwProtocolHandler)};
    return server;
}

void NetwServer::HandleCommand(Poller &poller, NetwFdOutputStruct &outputBuffers) {
    while (!commandBuffer.empty()) {
        auto ch = commandBuffer[0];
        commandBuffer.erase(0);
        if (ch == 'q') {
            quitCommandReceived = true;
        } else if (ch == 'w') {
            std::vector<NetwFdOutput> buffers{};
            {
                std::lock_guard lock{outputBuffers.mtx};
                for (auto &buffer : outputBuffers.buffers) {
                    buffers.emplace_back(std::move(buffer));
                }
                outputBuffers.buffers.clear();
                outputBuffers.signaled = false;
            }
            for (auto &buffer : buffers) {
                auto iterator = clients.begin();
                while (iterator != clients.end()) {
                    auto &clientFd = *iterator;
                    if (buffer.id == clientFd.id) {
                        if (!buffer.chunk.empty()) {
                            clientFd.outputBuffer.append(buffer.chunk);
                        }
                        bool closed{false};
                        if (buffer.close) {
                            if (!clientFd.outputBuffer.empty()) {
                                clientFd.closeSocket = true;
                            } else {
                                poller.RemoveFd(clientFd.fd);
                                iterator = clients.erase(iterator);
                                break;
                            }
                        }
                        poller.UpdateFd(clientFd.fd, true, !clientFd.outputBuffer.empty());
                        break;
                    }
                    ++iterator;
                }
            }
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

static uint64_t netwClientId{0};

task<void> NetwServer::ConnectionAcceptLoop(const std::shared_ptr<Poller> &pollerIn, const std::shared_ptr<NetwServer> &selfptrIn) {
    std::shared_ptr<Poller> poller{pollerIn};
    std::shared_ptr<NetwServer> selfptr{selfptrIn};
    std::shared_ptr<NetwFdOutputStruct> outputBuffers{this->outputBuffers};
    int commandFd = commandInput;
    while (!selfptr->quitAccepting) {
        std::cout << "Waiting for connections\n";
        co_await ConnectionAcceptReady(selfptr);
        if (selfptr->quitAccepting) {
            break;
        }
        auto clientFd = serverSocket.Accept();
        if (clientFd.IsValid()) {
            std::cout << "Accepted new connection " << clientFd << "\n";
            uint64_t id{netwClientId++};
            NetwClient cl{.id = id, .fd = std::move(clientFd), .inputBuffer = {}, .outputBuffer = {}, .handle = {netwProtocolHandler, netwProtocolHandler->Create([id, commandFd, outputBuffers] (const std::string &output) {
                NetwFdOutput buffer{.id = id, .chunk = output, .close = false};
                bool signal{false};
                {
                    std::lock_guard lock{outputBuffers->mtx};
                    outputBuffers->buffers.emplace_back(std::move(buffer));
                    signal = !outputBuffers->signaled;
                    if (signal) {
                        outputBuffers->signaled = true;
                    }
                }
                if (signal) {
                    write(commandFd, "w", 1);
                }
            }, [id, commandFd, outputBuffers] () {
                NetwFdOutput buffer{.id = id, .chunk = {}, .close = true};
                bool signal{false};
                {
                    std::lock_guard lock{outputBuffers->mtx};
                    outputBuffers->buffers.emplace_back(std::move(buffer));
                    signal = !outputBuffers->signaled;
                    if (signal) {
                        outputBuffers->signaled = true;
                    }
                }
                if (signal) {
                    write(commandFd, "w", 1);
                }
            })}};
            auto &fd = clients.emplace_back(std::move(cl));
            poller->AddFd(fd.fd, true, !fd.outputBuffer.empty(), true);
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

task<void> NetwServer::CommandReadLoop(const std::shared_ptr<Poller> &pollerIn, const std::shared_ptr<NetwServer> &selfptrIn) {
    std::shared_ptr<Poller> poller{pollerIn};
    std::shared_ptr<NetwServer> selfptr{selfptrIn};
    std::shared_ptr<NetwFdOutputStruct> outputBuffers{this->outputBuffers};
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
                selfptr->HandleCommand(*poller, *outputBuffers);
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
    std::string buf{};
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
                    auto serverReadyTpl = poller->GetResults(serverSocket);
                    auto serverReady = std::get<0>(serverReadyTpl) || std::get<2>(serverReadyTpl);
                    if (serverReady) {
                        std::vector<std::function<void()>> cbs{};
                        for (const auto &cb: acceptReadyCallback) {
                            cbs.emplace_back(cb);
                        }
                        acceptReadyCallback.clear();
                        for (const auto &cb: cbs) {
                            cb();
                        }
                    }
                    auto iterator = clients.begin();
                    while (iterator != clients.end()) {
                        auto &client = *iterator;
                        auto fdReadyTpl = poller->GetResults(client.fd);
                        if (std::get<1>(fdReadyTpl)) {
                            try {
                                auto wrCount = client.fd.Write(client.outputBuffer);
                                if (wrCount > 0) {
                                    client.outputBuffer.erase(0, wrCount);
                                }
                                if (client.outputBuffer.empty() && client.closeSocket) {
                                    std::cout << "Finished writing to socket, closing\n";
                                    poller->RemoveFd(client.fd);
                                    iterator = clients.erase(iterator);
                                    continue;
                                }
                            } catch (const FdException &e) {
                                std::cout<< "Error writing to socket, closing\n";
                                poller->RemoveFd(client.fd);
                                iterator = clients.erase(iterator);
                                continue;
                            }
                        }
                        if (std::get<0>(fdReadyTpl) || std::get<2>(fdReadyTpl)) {
                            buf.resize(8192);
                            try {
                                auto rdCount = client.fd.Read(buf);
                                if (rdCount > 0) {
                                    std::cout << "Read " << rdCount << " bytes.\n";
                                    buf.resize(rdCount);
                                    client.inputBuffer.append(buf);
                                }
                            } catch (const FdException &e) {
                                std::cout<< "Error reading from socket, closing\n";
                                poller->RemoveFd(client.fd);
                                iterator = clients.erase(iterator);
                                continue;
                            }
                        }
                        if (!client.inputBuffer.empty()) {
                            auto consumed = client.handle.AcceptInput(client.inputBuffer);
                            if (consumed > 0) {
                                client.inputBuffer.erase(0, consumed);
                            }
                        }
                        poller->UpdateFd(client.fd, true, !client.outputBuffer.empty());
                        ++iterator;
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
    FireAndForget<task<void>>([shrptr, poller] () -> task<void> { return shrptr->ConnectionAcceptLoop(poller, shrptr); });
    FireAndForget<task<void>>([shrptr, poller] () -> task<void> { return shrptr->CommandReadLoop(poller, shrptr); });
    FireAndForget<task<void>>([shrptr, poller] () -> task<void> { return shrptr->PollLoop(shrptr, poller); });
    while (!quitLoop) {
        poller->Runner();
    }
}