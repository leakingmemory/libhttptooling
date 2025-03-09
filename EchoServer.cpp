//
// Created by sigsegv on 3/9/25.
//

#include "EchoServer.h"

class EchoConnectionHandler : public NetwConnectionHandler {
private:
    std::function<void(const std::string &)> output;
    std::function<void()> close;
public:
    EchoConnectionHandler(const std::function<void(const std::string &)> &output, const std::function<void()> &close) : output(output), close(close) {}
    size_t AcceptInput(const std::string &) override;
};

size_t EchoConnectionHandler::AcceptInput(const std::string &input) {
    output(input);
    return input.size();
}

NetwConnectionHandler *
EchoServer::Create(const std::function<void(const std::string &)> &output, const std::function<void()> &close) {
    return new EchoConnectionHandler(output, close);
}

void EchoServer::Release(NetwConnectionHandler *handler) {
    delete handler;
}