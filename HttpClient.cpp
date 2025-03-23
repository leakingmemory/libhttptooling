//
// Created by sigsegv on 3/16/25.
//

#include "HttpClient.h"
#include "HttpClientImpl.h"
#include "NetwServer.h"
extern "C" {
#include <unistd.h>
}

HttpClient::HttpClient() :
    clientImpl(std::make_shared<HttpClientImpl>()),
    netwServer(NetwServer::Create(clientImpl)),
    commandFd(netwServer->GetCommandFd()) {
}

std::shared_ptr<HttpClient> HttpClient::Create() {
    std::shared_ptr<HttpClient> client{new HttpClient()};
    return client;
}

std::shared_ptr<HttpRequest> HttpClient::Request(const std::string &method, const std::string &path) {
    return clientImpl->Request(method, path);
}

task<std::expected<std::shared_ptr<HttpResponse>,FdException>>
HttpClient::Execute(const std::string &host, int port, const std::shared_ptr<HttpRequest> &request) {
    return clientImpl->Execute(host, port, request);
}

void HttpClient::Stop() {
    write(commandFd, "q", 1);
}

void HttpClient::Run() {
    netwServer->Run();
}
