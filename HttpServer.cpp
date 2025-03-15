//
// Created by sigsegv on 3/9/25.
//

#include "HttpServer.h"
#include "Http1Protocol.h"
#include "HttpHeaders.h"

class HttpServerConnectionHandler;

struct HttpServerResponseContainer {
    std::weak_ptr<HttpServerConnectionHandler> handler{};
    std::string output{};
    bool completed{false};
};

class HttpRequestImpl;

class HttpServerConnectionHandler : public NetwConnectionHandler, public std::enable_shared_from_this<HttpServerConnectionHandler> {
private:
    std::weak_ptr<HttpServer> httpServer;
    std::function<void(const std::string &)> output;
    std::function<void()> close;
    Http1Request requestHead{};
    std::vector<std::shared_ptr<HttpServerResponseContainer>> inflightRequests{};
    std::shared_ptr<HttpRequestImpl> requestBodyPending{};
    size_t requestBodyRemaining{0};
    std::mutex mtx;
    bool closeConnection{};
public:
    HttpServerConnectionHandler(const std::shared_ptr<HttpServer> &httpServer, const std::function<void(const std::string &)> &output, const std::function<void()> &close) : httpServer(httpServer), output(output), close(close) {}
    size_t AcceptInput(const std::string &) override;
    void RunOutputs();
};

class HttpRequestImpl : public HttpRequest, public std::enable_shared_from_this<HttpRequestImpl> {
private:
    std::weak_ptr<HttpServerConnectionHandler> serverConnectionHandler;
    std::weak_ptr<HttpServerResponseContainer> serverResponseContainer;
    std::string method{};
    std::string path{};
    std::mutex mtx{};
    std::string requestBody{};
    std::vector<std::function<void ()>> callRequestBodyFinished{};
    bool requestBodyComplete;
public:
    HttpRequestImpl(const std::shared_ptr<HttpServerConnectionHandler> &serverConnectionHandler, std::shared_ptr<HttpServerResponseContainer> &serverResponseContainer, const std::string &method, const std::string &path, bool hasRequestBody) : serverConnectionHandler(serverConnectionHandler), serverResponseContainer(serverResponseContainer), method(method), path(path), requestBodyComplete(!hasRequestBody) {
        std::transform(this->method.cbegin(), this->method.cend(), this->method.begin(), [] (char ch) {return std::toupper(ch);});
    }
    std::string GetMethod() const override;
    std::string GetPath() const override;
    void Respond(const std::shared_ptr<HttpResponse> &) override;
    task<std::string> RequestBody() override;
    void RecvBody(const std::string &chunk);
    void CompletedBody();
};

std::string HttpRequestImpl::GetMethod() const {
    return method;
}

std::string HttpRequestImpl::GetPath() const {
    return path;
}

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

void HttpRequestImpl::RecvBody(const std::string &chunk) {
    std::lock_guard lock{mtx};
    requestBody.append(chunk);
}

void HttpRequestImpl::CompletedBody() {
    {
        std::lock_guard lock{mtx};
        requestBodyComplete = true;
    }
    for (const auto &cl : callRequestBodyFinished) {
        cl();
    }
}

task<std::string> HttpRequestImpl::RequestBody() {
    {
        std::unique_lock lock{mtx};
        if (!requestBodyComplete) {
            lock.unlock();
            std::weak_ptr<HttpRequestImpl> reqObj{shared_from_this()};
            func_task<std::string> fnTask{[reqObj] (const auto &func) {
                auto req = reqObj.lock();
                std::unique_lock lock{req->mtx};
                if (req->requestBodyComplete) {
                    lock.unlock();
                    func(req->requestBody);
                    return;
                }
                req->callRequestBodyFinished.emplace_back([reqObj, func] () {
                    auto req = reqObj.lock();
                    if (req) {
                        func(req->requestBody);
                    } else {
                        func("");
                    }
                });
            }};
            auto reqBody = co_await fnTask;
            co_return reqBody;
        }
    }
    co_return requestBody;
}

size_t HttpServerConnectionHandler::AcceptInput(const std::string &input) {
    if (closeConnection) {
        return input.size();
    }
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