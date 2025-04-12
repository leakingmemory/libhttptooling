//
// Created by sigsegv on 3/16/25.
//

#include "HttpRequestImpl.h"
#include "Http1Protocol.h"
#include "HttpServerResponseContainer.h"
#include "HttpServerConnectionHandler.h"
#include <vector>

std::string HttpRequestImpl::GetContent() const {
    return requestBody;
}

std::string HttpRequestImpl::GetContentType() const {
    return contentType;
}

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

void HttpRequestImpl::FailedBody() {
    {
        std::lock_guard lock{mtx};
        requestBodyComplete = true;
        requestBodyFailed = true;
    }
    for (const auto &cl : callRequestBodyFinished) {
        cl();
    }
}

task<HttpRequestBody> HttpRequestImpl::RequestBody() {
    {
        std::unique_lock lock{mtx};
        if (!requestBodyComplete) {
            lock.unlock();
            std::weak_ptr<HttpRequestImpl> reqObj{shared_from_this()};
            func_task<HttpRequestBody> fnTask{[reqObj] (const auto &func) {
                auto req = reqObj.lock();
                std::unique_lock lock{req->mtx};
                if (req->requestBodyComplete) {
                    lock.unlock();
                    func({.content = req->requestBody, .success = !req->requestBodyFailed});
                    return;
                }
                req->callRequestBodyFinished.emplace_back([reqObj, func] () {
                    auto req = reqObj.lock();
                    if (req) {
                        func({.content = req->requestBody, .success = !req->requestBodyFailed});
                    } else {
                        func({.content = "", .success = false});
                    }
                });
            }};
            auto reqBody = co_await fnTask;
            co_return reqBody;
        }
    }
    co_return {.content = requestBody, .success = !requestBodyFailed};
}

void HttpRequestImpl::SetContent(const std::string &content, const std::string &contentType) {
    requestBody = content;
    this->contentType = contentType;
}
