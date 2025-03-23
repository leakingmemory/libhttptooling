//
// Created by sigsegv on 3/9/25.
//

#ifndef LIBHTTPTOOLING_HTTPSERVERIMPL_H
#define LIBHTTPTOOLING_HTTPSERVERIMPL_H

#include "include/task.h"
#include "NetwServer.h"
#include "HttpResponse.h"
#include "HttpRequest.h"

class HttpServerConnectionHandler;

class HttpServerImpl : public NetwProtocolHandler, public std::enable_shared_from_this<HttpServerImpl> {
    friend HttpServerConnectionHandler;
private:
    std::vector<std::shared_ptr<HttpRequest>> requestQueue{};
    std::vector<std::function<void (const std::shared_ptr<HttpRequest> &)>> requestHandlerQueue{};
    std::mutex mtx;
public:
    NetwConnectionHandler *Create(const std::function<void (const std::string &)> &output, const std::function<void ()> &close) override;
    void Release(NetwConnectionHandler *) override;
    void SetAssociatedNetwServer(const std::weak_ptr<NetwServer> &) override;
    task<std::shared_ptr<HttpRequest>> NextRequest();
};


#endif //LIBHTTPTOOLING_HTTPSERVERIMPL_H
