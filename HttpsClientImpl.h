//
// Created by sigsegv on 4/12/25.
//

#ifndef LIBHTTPTOOLING_HTTPSCLIENTIMPL_H
#define LIBHTTPTOOLING_HTTPSCLIENTIMPL_H


#include "HttpClientImpl.h"

class HttpsClientImpl : public NetwProtocolHandler, public NetwServerInterface, public std::enable_shared_from_this<HttpsClientImpl> {
private:
    std::shared_ptr<NetwProtocolHandler> upstreamHandler;
    std::weak_ptr<NetwServerInterface> netwServer;
    HttpClientImpl httpClientImpl{};
public:
    HttpsClientImpl(const std::shared_ptr<NetwProtocolHandler> &upstreamHandler) : upstreamHandler(upstreamHandler) {}
    NetwConnectionHandler *Create(const std::function<void (const std::string &)> &output, const std::function<void ()> &close) override;
    void Release(NetwConnectionHandler *) override;
    void SetAssociatedNetwServer(const std::weak_ptr<NetwServerInterface> &) override;
    void Connect(const void *ipaddr_norder, size_t ipaddr_len, int port, const std::string &requestData, const std::function<void (NetwConnectionHandler *)> &);
};


#endif //LIBHTTPTOOLING_HTTPSCLIENTIMPL_H
