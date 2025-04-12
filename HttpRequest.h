//
// Created by sigsegv on 3/15/25.
//

#ifndef LIBHTTPTOOLING_HTTPREQUEST_H
#define LIBHTTPTOOLING_HTTPREQUEST_H

#include "HttpResponse.h"
#include "include/task.h"
#include <memory>

class HttpClientImpl;

struct HttpRequestBody {
    std::string content{};
    bool success{true};
};

class HttpRequest {
    friend HttpClientImpl;
protected:
    virtual std::string GetContent() const = 0;
    virtual std::string GetContentType() const = 0;
public:
    virtual ~HttpRequest() = default;
    virtual std::string GetMethod() const = 0;
    virtual std::string GetPath() const = 0;
    virtual void Respond(const std::shared_ptr<HttpResponse> &) = 0;
    virtual task<HttpRequestBody> RequestBody() = 0;
    virtual void SetContent(const std::string &content, const std::string &contentType) = 0;
};

#endif //LIBHTTPTOOLING_HTTPREQUEST_H
