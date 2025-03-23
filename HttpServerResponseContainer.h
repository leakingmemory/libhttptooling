//
// Created by sigsegv on 3/16/25.
//

#ifndef LIBHTTPTOOLING_HTTPSERVERRESPONSECONTAINER_H
#define LIBHTTPTOOLING_HTTPSERVERRESPONSECONTAINER_H

#include <memory>
#include <string>

class HttpServerConnectionHandler;

struct HttpServerResponseContainer {
    std::weak_ptr<HttpServerConnectionHandler> handler{};
    std::string output{};
    bool completed{false};
};

#endif //LIBHTTPTOOLING_HTTPSERVERRESPONSECONTAINER_H
