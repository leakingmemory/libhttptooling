//
// Created by sigsegv on 3/9/25.
//

#include "HttpServerImpl.h"
#include "Http1Protocol.h"
#include "HttpHeaders.h"
#include "HttpServerResponseContainer.h"
#include "HttpServerConnectionHandler.h"
#include "HttpRequestImpl.h"

size_t HttpServerConnectionHandler::AcceptInput(const std::string &input) {
    if (requestBodyRemaining > 0) {
        if (input.size() <= requestBodyRemaining) {
            requestBodyPending->RecvBody(input);
            requestBodyRemaining -= input.size();
            if (requestBodyRemaining <= 0) {
                requestBodyPending->CompletedBody();
                requestBodyPending = {};
            }
            return input.size();
        } else {
            auto len = requestBodyRemaining;
            auto chunk = input.substr(0, len);
            requestBodyPending->RecvBody(chunk);
            requestBodyRemaining = 0;
            requestBodyPending->CompletedBody();
            requestBodyPending = {};
            return len;
        }
    }
    if (closeConnection) {
        return input.size();
    }
    Http1RequestParser parser{input};
    if (parser.IsValid()) {
        requestHead = parser.operator Http1Request();
        HttpHeaderValues hdrValues{requestHead};
        size_t contentLength = hdrValues.ContentLength;
        bool hasRequestBody = contentLength > 0;
        if (hasRequestBody) {
            auto method = requestHead.GetRequest().GetMethod();
            std::transform(method.cbegin(), method.cend(), method.begin(), [] (char ch) { return std::tolower(ch); });
            if (method == "get" || method == "head") {
                Http1Response response{{"HTTP/1.1", 400, "Bad request"}, {{"Content-Length", "0"}, {"Connection", "close"}}};
                std::weak_ptr<HttpServerConnectionHandler> weakPtr{shared_from_this()};
                HttpServerResponseContainer resp{.handler = std::move(weakPtr), .output = response.operator std::string(), .completed = true};
                {
                    std::lock_guard lock{mtx};
                    inflightRequests.emplace_back(std::make_shared<HttpServerResponseContainer>(std::move(resp)));
                    closeConnection = true;
                }
                RunOutputs();
                return parser.GetParsedInputCharacters();
            }
        }
        auto httpServer = this->httpServer.lock();
        if (httpServer) {
            auto container = std::make_shared<HttpServerResponseContainer>();
            container->handler = shared_from_this();
            std::function<void (const std::shared_ptr<HttpRequest> &)> postRequest{[] (auto req) {}};
            auto req = std::make_shared<HttpRequestImpl>(shared_from_this(), container, requestHead.GetRequest().GetMethod(), requestHead.GetRequest().GetPath(), hasRequestBody);
            if (hasRequestBody) {
                requestBodyPending = req;
                requestBodyRemaining = contentLength;
            }
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
                    httpServer->requestQueue.emplace_back(req);
                }
            }
            postRequest(req);
        } else {
            Http1Response response{{"HTTP/1.1", 503, "Service unavailable"},
                                   {{"Content-Length", "0"}, {"Connection", "close"}}};
            std::weak_ptr<HttpServerConnectionHandler> weakPtr{shared_from_this()};
            HttpServerResponseContainer resp{.handler = std::move(
                    weakPtr), .output = response.operator std::string(), .completed = true};
            {
                std::lock_guard lock{mtx};
                inflightRequests.emplace_back(std::make_shared<HttpServerResponseContainer>(std::move(resp)));
                closeConnection = true;
            }
            RunOutputs();
        }
        return parser.GetParsedInputCharacters();
    } else if (!parser.IsTruncatedValid()) {
        Http1Response response{{"HTTP/1.1", 400, "Bad request"}, {{"Content-Length", "0"}, {"Connection", "close"}}};
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
    HttpServerConnectionHandlerProxy(const std::shared_ptr<HttpServerImpl> &httpServer, const std::function<void(const std::string &)> &output, const std::function<void()> &close) : handler(std::make_shared<HttpServerConnectionHandler>(httpServer, output, close)) {}
    size_t AcceptInput(const std::string &) override;
};

size_t HttpServerConnectionHandlerProxy::AcceptInput(const std::string &input) {
    return handler->AcceptInput(input);
}

NetwConnectionHandler *
HttpServerImpl::Create(const std::function<void(const std::string &)> &output, const std::function<void()> &close) {
    std::shared_ptr<HttpServerImpl> shptr = shared_from_this();
    return new HttpServerConnectionHandlerProxy(shptr, output, close);
}

void HttpServerImpl::Release(NetwConnectionHandler *handler) {
    delete handler;
}

void HttpServerImpl::SetAssociatedNetwServer(const std::weak_ptr<NetwServer> &) {
}

task<std::shared_ptr<HttpRequest>> HttpServerImpl::NextRequest() {
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