//
// Created by sigsegv on 3/9/25.
//

#ifndef LIBHTTPTOOLING_ECHOSERVER_H
#define LIBHTTPTOOLING_ECHOSERVER_H

#include "NetwServer.h"

class EchoServer : public NetwProtocolHandler {
public:
    NetwConnectionHandler *Create(const std::function<void (const std::string &)> &output, const std::function<void ()> &close) override;
    void Release(NetwConnectionHandler *) override;
};


#endif //LIBHTTPTOOLING_ECHOSERVER_H
