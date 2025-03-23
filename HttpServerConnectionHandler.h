//
// Created by sigsegv on 3/16/25.
//

#ifndef LIBHTTPTOOLING_HTTPSERVERCONNECTIONHANDLER_H
#define LIBHTTPTOOLING_HTTPSERVERCONNECTIONHANDLER_H

#include "NetwServer.h"
#include "Http1Protocol.h"

class HttpServerImpl;
class HttpServerResponseContainer;
class HttpRequestImpl;

class HttpServerConnectionHandler : public NetwConnectionHandler, public std::enable_shared_from_this<HttpServerConnectionHandler> {
private:
    std::weak_ptr<HttpServerImpl> httpServer;
    std::function<void(const std::string &)> output;
    std::function<void()> close;
    Http1Request requestHead{};
    std::vector<std::shared_ptr<HttpServerResponseContainer>> inflightRequests{};
    std::shared_ptr<HttpRequestImpl> requestBodyPending{};
    size_t requestBodyRemaining{0};
    std::mutex mtx;
    bool closeConnection{};
public:
    HttpServerConnectionHandler(const std::shared_ptr<HttpServerImpl> &httpServer, const std::function<void(const std::string &)> &output, const std::function<void()> &close) : httpServer(httpServer), output(output), close(close) {}
    size_t AcceptInput(const std::string &) override;
    void RunOutputs();
};

#endif //LIBHTTPTOOLING_HTTPSERVERCONNECTIONHANDLER_H
