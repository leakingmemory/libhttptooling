//
// Created by sigsegv on 3/16/25.
//

#ifndef LIBHTTPTOOLING_HTTPCLIENT_H
#define LIBHTTPTOOLING_HTTPCLIENT_H

#include <memory>
#include <string>
#include <expected>
#include "Fd.h"
#include "HttpRequest.h"
#include "HttpResponse.h"

class NetwServer;
class HttpClientImpl;

class HttpClient {
private:
    std::shared_ptr<HttpClientImpl> clientImpl;
    std::shared_ptr<NetwServer> netwServer;
    int commandFd;
    HttpClient();
public:
    HttpClient(const HttpClient &) = delete;
    HttpClient(HttpClient &&) = delete;
    HttpClient &operator =(const HttpClient &) = delete;
    HttpClient &operator =(HttpClient &&) = delete;
    static std::shared_ptr<HttpClient> Create();
    std::shared_ptr<HttpRequest> Request(const std::string &method, const std::string &path);
    task<std::expected<std::shared_ptr<HttpResponse>,FdException>> Execute(const std::string &host, int port, const std::shared_ptr<HttpRequest> &request);
    void Stop();
    void Run();
};


#endif //LIBHTTPTOOLING_HTTPCLIENT_H
