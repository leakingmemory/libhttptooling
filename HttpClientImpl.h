//
// Created by sigsegv on 3/16/25.
//

#ifndef LIBHTTPTOOLING_HTTPCLIENTIMPL_H
#define LIBHTTPTOOLING_HTTPCLIENTIMPL_H


#include "NetwServer.h"
#include "HttpResponse.h"
#include "HttpRequest.h"
#include <expected>
#include "Fd.h"

class HttpClientConnectionHandler;

class HttpClientImpl : public NetwProtocolHandler, public std::enable_shared_from_this<HttpClientImpl> {
private:
    std::weak_ptr<NetwServer> netwServer{};
public:
    NetwConnectionHandler *Create(const std::function<void (const std::string &)> &output, const std::function<void ()> &close) override;
    void Release(NetwConnectionHandler *) override;
    void SetAssociatedNetwServer(const std::weak_ptr<NetwServer> &netwServer) override;
    std::shared_ptr<HttpRequest> Request(const std::string &method, const std::string &path);
    task<std::expected<std::shared_ptr<HttpResponse>,FdException>> Execute(const std::string &host, int port, const std::shared_ptr<HttpRequest> &request);
};


#endif //LIBHTTPTOOLING_HTTPCLIENTIMPL_H
