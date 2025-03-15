//
// Created by sigsegv on 3/15/25.
//

#include "HttpServer.h"
#include "HttpServerImpl.h"
extern "C" {
#include <unistd.h>
}

HttpServer::HttpServer(int port) :
    serverImpl(std::make_shared<HttpServerImpl>()),
    netwServer(NetwServer::Create(port, serverImpl)),
    commandFd(netwServer->GetCommandFd()) {
}

std::shared_ptr<HttpServer> HttpServer::Create(int port) {
    std::shared_ptr<HttpServer> server{new HttpServer(port)};
    return server;
}

task<std::shared_ptr<HttpRequest>> HttpServer::NextRequest() {
    return serverImpl->NextRequest();
}

void HttpServer::Stop() {
    write(commandFd, "q", 1);
}

void HttpServer::Run() {
    netwServer->Run();
}
