//
// Created by sigsegv on 3/15/25.
//

#ifndef LIBHTTPTOOLING_HTTPREQUEST_H
#define LIBHTTPTOOLING_HTTPREQUEST_H

#include "HttpResponse.h"
#include "include/task.h"
#include <memory>

class HttpRequest {
public:
    virtual ~HttpRequest() = default;
    virtual std::string GetMethod() const = 0;
    virtual std::string GetPath() const = 0;
    virtual void Respond(const std::shared_ptr<HttpResponse> &) = 0;
    virtual task<std::string> RequestBody() = 0;
};

#endif //LIBHTTPTOOLING_HTTPREQUEST_H
