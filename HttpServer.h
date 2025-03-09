//
// Created by sigsegv on 3/9/25.
//

#ifndef LIBHTTPTOOLING_HTTPSERVER_H
#define LIBHTTPTOOLING_HTTPSERVER_H


#include "NetwServer.h"

class HttpServer : public NetwProtocolHandler {
public:
    NetwConnectionHandler *Create(const std::function<void (const std::string &)> &output, const std::function<void ()> &close) override;
    void Release(NetwConnectionHandler *) override;
};


#endif //LIBHTTPTOOLING_HTTPSERVER_H
