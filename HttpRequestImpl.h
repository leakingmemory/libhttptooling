//
// Created by sigsegv on 3/16/25.
//

#ifndef LIBHTTPTOOLING_HTTPREQUESTIMPL_H
#define LIBHTTPTOOLING_HTTPREQUESTIMPL_H

#include "HttpRequest.h"
#include <memory>
#include <mutex>

class HttpClientImpl;
class HttpServerConnectionHandler;
class HttpServerResponseContainer;

class HttpRequestImpl : public HttpRequest, public std::enable_shared_from_this<HttpRequestImpl> {
    friend HttpClientImpl;
private:
    std::weak_ptr<HttpServerConnectionHandler> serverConnectionHandler;
    std::weak_ptr<HttpServerResponseContainer> serverResponseContainer;
    std::string method{};
    std::string path{};
    std::mutex mtx{};
    std::string requestBody{};
    std::string contentType{};
    std::vector<std::function<void ()>> callRequestBodyFinished{};
    bool requestBodyComplete;
    bool requestBodyFailed{false};
public:
    HttpRequestImpl(const std::shared_ptr<HttpServerConnectionHandler> &serverConnectionHandler, std::shared_ptr<HttpServerResponseContainer> &serverResponseContainer, const std::string &method, const std::string &path, bool hasRequestBody) : serverConnectionHandler(serverConnectionHandler), serverResponseContainer(serverResponseContainer), method(method), path(path), requestBodyComplete(!hasRequestBody) {
        std::transform(this->method.cbegin(), this->method.cend(), this->method.begin(), [] (char ch) {return std::toupper(ch);});
    }
    HttpRequestImpl(const std::string &method, const std::string &path) : serverConnectionHandler(), serverResponseContainer(), method(method), path(path), requestBodyComplete(true) {
        std::transform(this->method.cbegin(), this->method.cend(), this->method.begin(), [] (char ch) {return std::toupper(ch);});
    }
protected:
    std::string GetContent() const override;
    std::string GetContentType() const override;
public:
    std::string GetMethod() const override;
    std::string GetPath() const override;
    void Respond(const std::shared_ptr<HttpResponse> &) override;
    task<HttpRequestBody> RequestBody() override;
    void RecvBody(const std::string &chunk);
    void CompletedBody();
    void FailedBody();
    void SetContent(const std::string &content, const std::string &contentType) override;
};

#endif //LIBHTTPTOOLING_HTTPREQUESTIMPL_H
