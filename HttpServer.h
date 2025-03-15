//
// Created by sigsegv on 3/15/25.
//

#ifndef LIBHTTPTOOLING_HTTPSERVER_H
#define LIBHTTPTOOLING_HTTPSERVER_H

#include "HttpRequest.h"
#include "include/task.h"
#include <memory>

class HttpServerImpl;
class NetwServer;

class HttpServer {
private:
    std::shared_ptr<HttpServerImpl> serverImpl;
    std::shared_ptr<NetwServer> netwServer;
    int commandFd;
private:
    HttpServer(int port);
public:
    HttpServer() = delete;
    HttpServer(const HttpServer &) = delete;
    HttpServer(HttpServer &&) = delete;
    HttpServer &operator = (const HttpServer &) = delete;
    HttpServer &operator = (HttpServer &&) = delete;
    static std::shared_ptr<HttpServer> Create(int port);
    task<std::shared_ptr<HttpRequest>> NextRequest();
    void Stop();
    void Run();
};


#endif //LIBHTTPTOOLING_HTTPSERVER_H
