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
    std::weak_ptr<HttpServer> httpServer;
    std::function<void(const std::string &)> output;
    std::function<void()> close;
    Http1Request requestHead{};
    std::vector<std::shared_ptr<HttpServerResponseContainer>> inflightRequests{};
    std::mutex mtx;
    bool closeConnection{};
public:
    HttpServerConnectionHandler(const std::shared_ptr<HttpServer> &httpServer, const std::function<void(const std::string &)> &output, const std::function<void()> &close) : httpServer(httpServer), output(output), close(close) {}
    size_t AcceptInput(const std::string &) override;
    void RunOutputs();
};

class HttpRequestImpl : public HttpRequest {
private:
    std::weak_ptr<HttpServerConnectionHandler> serverConnectionHandler;
    std::weak_ptr<HttpServerResponseContainer> serverResponseContainer;
public:
    HttpRequestImpl(const std::shared_ptr<HttpServerConnectionHandler> &serverConnectionHandler, std::shared_ptr<HttpServerResponseContainer> &serverResponseContainer) : serverConnectionHandler(serverConnectionHandler), serverResponseContainer(serverResponseContainer) {}
    void Respond(const std::shared_ptr<HttpResponse> &) override;
};

void HttpRequestImpl::Respond(const std::shared_ptr<HttpResponse> &response) {
    auto serverConnectionHandler = this->serverConnectionHandler.lock();
    auto serverResponseContainer = this->serverResponseContainer.lock();
    if (serverResponseContainer) {
        std::vector<Http1HeaderLine> hdrLns{};
        hdrLns.emplace_back("Content-Type", response->GetContentType());
        hdrLns.emplace_back("Content-Length", std::to_string(response->GetContentLength()));

        Http1Response responseHead{{"HTTP/1.1", response->GetCode(), response->GetDescription()}, hdrLns};
        serverResponseContainer->output.append(responseHead.operator std::string());
        serverResponseContainer->output.append(response->GetContent());
        serverResponseContainer->completed = true;
        if (serverConnectionHandler) {
            serverConnectionHandler->RunOutputs();
        }
    }
}

size_t HttpServerConnectionHandler::AcceptInput(const std::string &input) {
    if (closeConnection) {
        return input.size();
    }
    Http1RequestParser parser{input};
    if (parser.IsValid()) {
        requestHead = parser.operator Http1Request();
        auto httpServer = this->httpServer.lock();
        if (httpServer) {
            auto container = std::make_shared<HttpServerResponseContainer>();
            container->handler = shared_from_this();
            std::function<void (const std::shared_ptr<HttpRequest> &)> postRequest{[] (auto req) {}};
            {
                {
                    std::lock_guard lock{mtx};
                    inflightRequests.emplace_back(container);
                }
                std::lock_guard lock{httpServer->mtx};
                if (!httpServer->requestHandlerQueue.empty()) {
                    auto iterator = httpServer->requestHandlerQueue.begin();
                    postRequest = *iterator;
                    iterator = httpServer->requestHandlerQueue.erase(iterator);
                } else {
                    httpServer->requestQueue.emplace_back(std::make_shared<HttpRequestImpl>(shared_from_this(), container));
                }
            }
            postRequest(std::make_shared<HttpRequestImpl>(shared_from_this(), container));
        } else {
            Http1Response response{{"HTTP/1.1", 503, "Service unavailable"},
                                   {{"Content-Length", "0"}}};
            std::weak_ptr<HttpServerConnectionHandler> weakPtr{shared_from_this()};
            HttpServerResponseContainer resp{.handler = std::move(
                    weakPtr), .output = response.operator std::string(), .completed = true};
            {
                std::lock_guard lock{mtx};
                inflightRequests.emplace_back(std::make_shared<HttpServerResponseContainer>(std::move(resp)));
            }
            RunOutputs();
        }
        return parser.GetParsedInputCharacters();
    } else if (!parser.IsTruncatedValid()) {
        Http1Response response{{"HTTP/1.1", 400, "Bad request"}, {{"Content-Length", "0"}}};
        std::weak_ptr<HttpServerConnectionHandler> weakPtr{shared_from_this()};
        HttpServerResponseContainer resp{.handler = std::move(weakPtr), .output = response.operator std::string(), .completed = true};
        {
            std::lock_guard lock{mtx};
            inflightRequests.emplace_back(std::make_shared<HttpServerResponseContainer>(std::move(resp)));
            closeConnection = true;
        }
        RunOutputs();
        return input.size();
    }
    return 0;
}

void HttpServerConnectionHandler::RunOutputs() {
    std::vector<std::shared_ptr<HttpServerResponseContainer>> responses{};
    bool done;
    {
        std::lock_guard lock{mtx};
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
        done = inflightRequests.empty();
    }
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
    HttpServerConnectionHandlerProxy(const std::shared_ptr<HttpServer> &httpServer, const std::function<void(const std::string &)> &output, const std::function<void()> &close) : handler(std::make_shared<HttpServerConnectionHandler>(httpServer, output, close)) {}
    size_t AcceptInput(const std::string &) override;
};

size_t HttpServerConnectionHandlerProxy::AcceptInput(const std::string &input) {
    return handler->AcceptInput(input);
}

NetwConnectionHandler *
HttpServer::Create(const std::function<void(const std::string &)> &output, const std::function<void()> &close) {
    std::shared_ptr<HttpServer> shptr = shared_from_this();
    return new HttpServerConnectionHandlerProxy(shptr, output, close);
}

void HttpServer::Release(NetwConnectionHandler *handler) {
    delete handler;
}

task<std::shared_ptr<HttpRequest>> HttpServer::NextRequest() {
    auto shptr = shared_from_this();
    func_task<std::shared_ptr<HttpRequest>> ftask{[shptr] (const auto &callback) {
        std::shared_ptr<HttpRequest> req{};
        {
            std::lock_guard lock{shptr->mtx};
            if (shptr->requestQueue.empty()) {
                shptr->requestHandlerQueue.emplace_back([callback](const auto &reqIn) {
                    std::shared_ptr<HttpRequest> req{reqIn};
                    callback(req);
                });
                return;
            }
            auto iterator = shptr->requestQueue.begin();
            req = *iterator;
            iterator = shptr->requestQueue.erase(iterator);
        }
        callback(req);
    }};
    auto req = co_await ftask;
    co_return req;
}