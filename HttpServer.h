//
// Created by sigsegv on 3/9/25.
//

#ifndef LIBHTTPTOOLING_HTTPSERVER_H
#define LIBHTTPTOOLING_HTTPSERVER_H

#include "include/task.h"
#include "NetwServer.h"

class HttpResponse {
private:
    std::string content;
    std::string contentType;
    std::string description;
    int code;
public:
    constexpr HttpResponse(int code, const std::string &description) : code(code), description(description) {}
    constexpr HttpResponse(int code, std::string &&description) : code(code), description(std::move(description)) {}
    constexpr std::string GetContent() const {
        return content;
    }
    constexpr size_t GetContentLength() const {
        return content.size();
    }
    constexpr std::string GetContentType() const {
        return contentType;
    }
    constexpr std::string GetDescription() const {
        return description;
    }
    constexpr int GetCode() const {
        return code;
    }
    constexpr void SetContent(const std::string &content, const std::string &contentType){
        this->content = content;
        this->contentType = contentType;
    }
    constexpr void SetContent(std::string &&content, std::string &&contentType){
        this->content = std::move(content);
        this->contentType = std::move(contentType);
    }
};

class HttpServerConnectionHandler;

class HttpRequest {
public:
    virtual ~HttpRequest() = default;
    virtual void Respond(const std::shared_ptr<HttpResponse> &) = 0;
};

class HttpServer : public NetwProtocolHandler, public std::enable_shared_from_this<HttpServer> {
    friend HttpServerConnectionHandler;
private:
    std::vector<std::shared_ptr<HttpRequest>> requestQueue{};
    std::vector<std::function<void (const std::shared_ptr<HttpRequest> &)>> requestHandlerQueue{};
    std::mutex mtx;
public:
    NetwConnectionHandler *Create(const std::function<void (const std::string &)> &output, const std::function<void ()> &close) override;
    void Release(NetwConnectionHandler *) override;
    task<std::shared_ptr<HttpRequest>> NextRequest();
};


#endif //LIBHTTPTOOLING_HTTPSERVER_H
