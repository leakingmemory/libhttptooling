//
// Created by sigsegv on 4/15/25.
//

#ifndef LIBHTTPTOOLING_HTTPSCLIENT_H
#define LIBHTTPTOOLING_HTTPSCLIENT_H

#include <memory>
#include <string>
#include <expected>
#include "Fd.h"
#include "HttpRequest.h"
#include "HttpResponse.h"

class HttpsClientImpl;
class HttpClientImpl;
class NetwServer;

class HttpsClient {
private:
    std::shared_ptr<HttpClientImpl> httpClientImpl;
    std::shared_ptr<HttpsClientImpl> httpsClientImpl;
    std::shared_ptr<NetwServer> netwServer;
    int commandFd;
    HttpsClient();
public:
    HttpsClient(const HttpsClient &) = delete;
    HttpsClient(HttpsClient &&) = delete;
    HttpsClient &operator =(const HttpsClient &) = delete;
    HttpsClient &operator =(HttpsClient &&) = delete;
    static std::shared_ptr<HttpsClient> Create();
    std::shared_ptr<HttpRequest> Request(const std::string &method, const std::string &path);
    task<std::expected<std::shared_ptr<HttpResponse>,FdException>> Execute(const std::string &host, int port, const std::shared_ptr<HttpRequest> &request);
    void Stop();
    void Run();
};


#endif //LIBHTTPTOOLING_HTTPSCLIENT_H
