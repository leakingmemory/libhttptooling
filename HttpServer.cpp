//
// Created by sigsegv on 3/9/25.
//

#include "HttpServer.h"
#include "Http1Protocol.h"

class HttpServerConnectionHandler;

struct HttpServerResponseContainer {
    std::weak_ptr<HttpServerConnectionHandler> handler{};
    std::string output{};
    bool completed{false};
};

class HttpServerConnectionHandler : public NetwConnectionHandler, public std::enable_shared_from_this<HttpServerConnectionHandler> {
private:
    std::function<void(const std::string &)> output;
    std::function<void()> close;
    Http1Request requestHead{};
    std::vector<std::shared_ptr<HttpServerResponseContainer>> inflightRequests{};
    bool closeConnection{};
public:
    HttpServerConnectionHandler(const std::function<void(const std::string &)> &output, const std::function<void()> &close) : output(output), close(close) {}
    size_t AcceptInput(const std::string &) override;
    void RunOutputs();
};

size_t HttpServerConnectionHandler::AcceptInput(const std::string &input) {
    if (closeConnection) {
        return input.size();
    }
    Http1RequestParser parser{input};
    if (parser.IsValid()) {
        requestHead = parser.operator Http1Request();
        Http1Response response{{"HTTP/1.1", 501, "Not implemented"}, {{"Content-Length", "0"}}};
        std::weak_ptr<HttpServerConnectionHandler> weakPtr{shared_from_this()};
        HttpServerResponseContainer resp{.handler = std::move(weakPtr), .output = response.operator std::string(), .completed = true};
        inflightRequests.emplace_back(std::make_shared<HttpServerResponseContainer>(std::move(resp)));
        RunOutputs();
        return parser.GetParsedInputCharacters();
    } else if (!parser.IsTruncatedValid()) {
        Http1Response response{{"HTTP/1.1", 400, "Bad request"}, {{"Content-Length", "0"}}};
        std::weak_ptr<HttpServerConnectionHandler> weakPtr{shared_from_this()};
        HttpServerResponseContainer resp{.handler = std::move(weakPtr), .output = response.operator std::string(), .completed = true};
        inflightRequests.emplace_back(std::make_shared<HttpServerResponseContainer>(std::move(resp)));
        closeConnection = true;
        RunOutputs();
        return input.size();
    }
    return 0;
}

void HttpServerConnectionHandler::RunOutputs() {
    std::vector<std::shared_ptr<HttpServerResponseContainer>> responses{};
    auto iterator = inflightRequests.begin();
    while (iterator != inflightRequests.end()) {
        const auto &resp = *iterator;
        if (resp->completed) {
            responses.emplace_back(resp);
            iterator = inflightRequests.erase(iterator);
            continue;
        }
        break;
    }
    auto done = inflightRequests.empty();
    for (const auto &resp : responses) {
        if (!resp->output.empty()) {
            output(resp->output);
        }
    }
    if (closeConnection && done) {
        close();
    }
}

class HttpServerConnectionHandlerProxy : public NetwConnectionHandler {
private:
    std::shared_ptr<HttpServerConnectionHandler> handler;
public:
    HttpServerConnectionHandlerProxy(const std::function<void(const std::string &)> &output, const std::function<void()> &close) : handler(std::make_shared<HttpServerConnectionHandler>(output, close)) {}
    size_t AcceptInput(const std::string &) override;
};

size_t HttpServerConnectionHandlerProxy::AcceptInput(const std::string &input) {
    return handler->AcceptInput(input);
}

NetwConnectionHandler *
HttpServer::Create(const std::function<void(const std::string &)> &output, const std::function<void()> &close) {
    return new HttpServerConnectionHandlerProxy(output, close);
}

void HttpServer::Release(NetwConnectionHandler *handler) {
    delete handler;
}