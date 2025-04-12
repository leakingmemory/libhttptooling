//
// Created by sigsegv on 3/16/25.
//

#include "HttpClientImpl.h"
#include "Http1Protocol.h"
#include "HttpHeaders.h"
#include "HttpRequestImpl.h"
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>

struct HttpClientRequestContainer {
    std::weak_ptr<HttpClientConnectionHandler> handler{};
    std::function<void (std::shared_ptr<HttpResponse> &response)> callback{};
    std::string requestMethod{};
};

class HttpResponseImpl;

class HttpClientConnectionHandler : public NetwConnectionHandler, public std::enable_shared_from_this<HttpClientConnectionHandler> {
private:
    std::weak_ptr<HttpClientImpl> httpClient;
    std::function<void(const std::string &)> output;
    std::function<void()> close;
    Http1Response requestHead{};
    std::vector<std::shared_ptr<HttpClientRequestContainer>> inflightRequests{};
    std::shared_ptr<HttpResponseImpl> responseBodyPending{};
    size_t responseBodyRemaining{0};
    std::mutex mtx;
    bool closeConnection{};
public:
    HttpClientConnectionHandler(const std::shared_ptr<HttpClientImpl> &httpServer, const std::function<void(const std::string &)> &output, const std::function<void()> &close) : httpClient(httpClient), output(output), close(close) {}
    size_t AcceptInput(const std::string &) override;
    void EndOfConnection() override;
    void WaitForResponse(const std::string &requestMethod, const std::function<void (std::shared_ptr<HttpResponse> &response)> &callback);
};

class HttpResponseImpl : public HttpResponse, public std::enable_shared_from_this<HttpResponseImpl> {
private:
    std::weak_ptr<HttpClientConnectionHandler> clientConnectionHandler;
    std::weak_ptr<HttpClientRequestContainer> clientRequestContainer;
    std::mutex mtx{};
    std::string responseBody{};
    bool responseBodyOk{true};
    std::vector<std::function<void ()>> callResponseBodyFinished{};
    bool responseBodyComplete;
public:
    HttpResponseImpl(const std::shared_ptr<HttpClientConnectionHandler> &clientConnectionHandler, std::shared_ptr<HttpClientRequestContainer> &clientRequestContainer, int code, const std::string &description, bool hasResponseBody) : HttpResponse(code, description), clientConnectionHandler(clientConnectionHandler), clientRequestContainer(clientRequestContainer), responseBodyComplete(!hasResponseBody) {
    }
    task<ResponseBodyResult> ResponseBody() override;
    void RecvBody(const std::string &chunk);
    void CompletedBody();
    void FailedBody();
};

task<ResponseBodyResult> HttpResponseImpl::ResponseBody() {
    {
        std::unique_lock lock{mtx};
        if (!responseBodyComplete) {
            lock.unlock();
            std::weak_ptr<HttpResponseImpl> respObj{shared_from_this()};
            func_task<ResponseBodyResult> fnTask{[respObj] (const auto &func) {
                auto resp = respObj.lock();
                std::unique_lock lock{resp->mtx};
                if (resp->responseBodyComplete) {
                    lock.unlock();
                    func({.body = resp->responseBody, .success = resp->responseBodyOk});
                    return;
                }
                resp->callResponseBodyFinished.emplace_back([respObj, func] () {
                    auto resp = respObj.lock();
                    if (resp) {
                        func({.body = resp->responseBody, .success = resp->responseBodyOk});
                    } else {
                        func({.body = "", .success = false});
                    }
                });
            }};
            auto respBody = co_await fnTask;
            co_return respBody;
        }
    }
    co_return {.body = responseBody, .success = responseBodyOk};
}

void HttpResponseImpl::RecvBody(const std::string &chunk) {
    std::lock_guard lock{mtx};
    responseBody.append(chunk);
}

void HttpResponseImpl::CompletedBody() {
    {
        std::lock_guard lock{mtx};
        responseBodyComplete = true;
    }
    for (const auto &cl : callResponseBodyFinished) {
        cl();
    }
}

void HttpResponseImpl::FailedBody() {
    {
        std::lock_guard lock{mtx};
        responseBodyComplete = true;
        responseBodyOk = false;
    }
    for (const auto &cl : callResponseBodyFinished) {
        cl();
    }
}

size_t HttpClientConnectionHandler::AcceptInput(const std::string &input) {
    if (responseBodyRemaining > 0) {
        if (input.size() <= responseBodyRemaining) {
            responseBodyPending->RecvBody(input);
            responseBodyRemaining -= input.size();
            if (responseBodyRemaining <= 0) {
                responseBodyPending->CompletedBody();
                responseBodyPending = {};
            }
            return input.size();
        } else {
            auto len = responseBodyRemaining;
            auto chunk = input.substr(0, len);
            responseBodyPending->RecvBody(chunk);
            responseBodyRemaining = 0;
            responseBodyPending->CompletedBody();
            responseBodyPending = {};
            return len;
        }
    }
    {
        std::lock_guard lock{mtx};
        if (closeConnection) {
            return input.size();
        }
        if (inflightRequests.empty()) {
            closeConnection = true;
            return input.size();
        }
    }
    Http1ResponseParser parser{input};
    if (parser.IsValid()) {
        auto responseHead = parser.operator Http1Response();
        HttpHeaderValues hdrValues{responseHead};
        size_t contentLength = hdrValues.ContentLength;
        bool hasResponseBody = contentLength > 0;
        std::shared_ptr<HttpClientRequestContainer> requestContainer{};
        {
            auto iterator = inflightRequests.begin();
            requestContainer = *iterator;
            inflightRequests.erase(iterator);
        }
        hasResponseBody = hasResponseBody && requestContainer->requestMethod != "HEAD";
        auto response = std::make_shared<HttpResponseImpl>(shared_from_this(), requestContainer, responseHead.GetResponseLine().GetCode(), responseHead.GetResponseLine().GetDescription(), hasResponseBody);
        if (hasResponseBody) {
            responseBodyRemaining = contentLength;
            responseBodyPending = response;
        }
        std::shared_ptr<HttpResponse> genResponse{response};
        requestContainer->callback(genResponse);
        return parser.GetParsedInputCharacters();
    } else if (!parser.IsTruncated()) {
        decltype(this->inflightRequests) inflightRequests{};
        {
            std::lock_guard lock{mtx};
            closeConnection = true;
            inflightRequests.reserve(this->inflightRequests.size());
            for (auto &&req : this->inflightRequests) {
                inflightRequests.emplace_back(std::move(req));
            }
            this->inflightRequests.clear();
        }
        for (auto &req : inflightRequests) {
            std::shared_ptr<HttpResponse> response{};
            req->callback(response);
        }
    }
    return 0;
}

void HttpClientConnectionHandler::EndOfConnection() {
    if (responseBodyRemaining > 0) {
        responseBodyPending->FailedBody();
        responseBodyPending = {};
    }
    decltype(this->inflightRequests) inflightRequests{};
    {
        std::lock_guard lock{mtx};
        closeConnection = true;
        inflightRequests.reserve(this->inflightRequests.size());
        for (auto &&req : this->inflightRequests) {
            inflightRequests.emplace_back(std::move(req));
        }
        this->inflightRequests.clear();
    }
    for (auto &req : inflightRequests) {
        std::shared_ptr<HttpResponse> response{};
        req->callback(response);
    }
}

void HttpClientConnectionHandler::WaitForResponse(const std::string &requestMethod, const std::function<void (std::shared_ptr<HttpResponse> &response)> &callback) {
    auto shptr = shared_from_this();
    std::weak_ptr<HttpClientConnectionHandler> wkptr{shptr};
    HttpClientRequestContainer reqContainer{.handler = std::move(wkptr), .callback = callback, .requestMethod = requestMethod};
    inflightRequests.emplace_back(std::make_shared<HttpClientRequestContainer>(std::move(reqContainer)));
}

class HttpClientConnectionHandlerProxy : public NetwConnectionHandler {
private:
    std::shared_ptr<HttpClientConnectionHandler> handler;
public:
    HttpClientConnectionHandlerProxy(const std::shared_ptr<HttpClientImpl> &httpClient, const std::function<void(const std::string &)> &output, const std::function<void()> &close) : handler(std::make_shared<HttpClientConnectionHandler>(httpClient, output, close)) {}
    size_t AcceptInput(const std::string &) override;
    void EndOfConnection() override;
    std::shared_ptr<HttpClientConnectionHandler> GetHandler() const {
        return handler;
    }
};

size_t HttpClientConnectionHandlerProxy::AcceptInput(const std::string &chunk) {
    return handler->AcceptInput(chunk);
}

void HttpClientConnectionHandlerProxy::EndOfConnection() {
    handler->EndOfConnection();
}

NetwConnectionHandler *
HttpClientImpl::Create(const std::function<void(const std::string &)> &output, const std::function<void()> &close) {
    std::shared_ptr<HttpClientImpl> shptr = shared_from_this();
    return new HttpClientConnectionHandlerProxy(shptr, output, close);
}

void HttpClientImpl::Release(NetwConnectionHandler *handler) {
    delete handler;
}

void HttpClientImpl::SetAssociatedNetwServer(const std::weak_ptr<NetwServer> &netwServer) {
    this->netwServer = netwServer;
}

std::shared_ptr<HttpRequest> HttpClientImpl::Request(const std::string &method, const std::string &path) {
    return std::make_shared<HttpRequestImpl>(method, path);
}

#include <iostream>
task<std::expected<std::shared_ptr<HttpResponse>,FdException>> HttpClientImpl::Execute(const std::string &host, int port, const std::shared_ptr<HttpRequest> &request) {
    auto netwServer = this->netwServer.lock();
    if (!netwServer) {
        co_return {};
    }
    std::unique_ptr<struct addrinfo,std::function<void (struct addrinfo *)>> addrinfo{};
    {
        struct addrinfo *addrinfo_raw;
        auto err = getaddrinfo(host.c_str(), NULL, NULL, &addrinfo_raw);
        if (err != 0) {
            throw std::exception();
        }
        std::unique_ptr<struct addrinfo,std::function<void (struct addrinfo *)>> up{addrinfo_raw, [] (struct addrinfo *ptr) {
            if (ptr != NULL) {
                freeaddrinfo(ptr);
            }
        }};
        addrinfo = std::move(up);
    }
    struct sockaddr *sa;
    {
        struct addrinfo *addrinfo_w = &(*addrinfo);
        while (addrinfo_w != NULL) {
            if (addrinfo_w->ai_addr != NULL) {
                sa = addrinfo_w->ai_addr;
                break;
            }
            addrinfo_w = addrinfo_w->ai_next;
        }
        if (sa == NULL) {
            throw std::exception();
        }
    }

    auto requestMethod = request->GetMethod();
    std::string reqContent{};
    {
        Http1RequestLine reqLine{requestMethod, request->GetPath(), "HTTP/1.1"};
        auto requestBody = request->GetContent();
        auto contentType = request->GetContentType();
        std::vector<Http1HeaderLine> header{};
        if (!requestBody.empty()) {
            if (!contentType.empty()) {
                header.emplace_back("Content-Type", contentType);
            }
            header.emplace_back("Content-Length", std::to_string(requestBody.size()));
        }
        header.emplace_back("Host", host);
        Http1Request request{reqLine, header};
        reqContent = request.operator std::string();
        if (!requestBody.empty()) {
            reqContent.append(requestBody);
        }
    }

    std::string addr{};
    if (sa->sa_family == AF_INET) {
        struct sockaddr_in *sa4 = (struct sockaddr_in *) sa;
        addr.resize(4);
        memcpy(addr.data(), &(sa4->sin_addr), 4);
    } else if (sa->sa_family == AF_INET6) {
        struct sockaddr_in6 *sa6 = (struct sockaddr_in6 *) sa;
        addr.resize(16);
        memcpy(addr.data(), &(sa6->sin6_addr), 16);
    }

    func_task<std::expected<std::shared_ptr<HttpResponse>,FdException>> fnTask{[netwServer, requestMethod, addr, port, reqContent] (const auto &callback) {
        std::function<void (NetwConnectionHandler *)> setupHandler{[requestMethod, callback] (NetwConnectionHandler *rawHandler) {
            HttpClientConnectionHandlerProxy *handlerProxy = dynamic_cast<HttpClientConnectionHandlerProxy *>(rawHandler);
            if (handlerProxy == nullptr) {
                throw std::exception();
            }
            auto handler = handlerProxy->GetHandler();
            handler->WaitForResponse(requestMethod, callback);
        }};
        try {
            netwServer->Connect(addr.data(), addr.size(), port, reqContent, setupHandler);
        } catch (const FdException &e) {
            callback(std::unexpected(e));
        }
    }};
    auto response = co_await fnTask;
    co_return response;
}