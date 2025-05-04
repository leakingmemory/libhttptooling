//
// Created by sigsegv on 4/15/25.
//

#include "HttpsClient.h"
#include "HttpClient.h"
#include "HttpClientImpl.h"
#include "HttpsClientImpl.h"
#include "NetwServer.h"
extern "C" {
#include <unistd.h>
#include <openssl/ssl.h>
}

class HttpClientSslInit {
public:
    HttpClientSslInit() {
        SSL_library_init();
    }
};

static HttpClientSslInit sslInit{};


HttpsClient::HttpsClient() :
        httpClientImpl(std::make_shared<HttpClientImpl>()),
        httpsClientImpl(std::make_shared<HttpsClientImpl>(httpClientImpl)),
        netwServer(NetwServer::Create(httpsClientImpl)),
        commandFd(netwServer->GetCommandFd()) {
    httpClientImpl->SetAssociatedNetwServer(httpsClientImpl);
    httpsClientImpl->SetAssociatedNetwServer(netwServer);
}

std::shared_ptr<HttpsClient> HttpsClient::Create() {
    std::shared_ptr<HttpsClient> client{new HttpsClient()};
    return client;
}

std::shared_ptr<HttpRequest> HttpsClient::Request(const std::string &method, const std::string &path) {
    return httpClientImpl->Request(method, path);
}

task<std::expected<std::shared_ptr<HttpResponse>,FdException>>
HttpsClient::Execute(const std::string &host, int port, const std::shared_ptr<HttpRequest> &request) {
    return httpClientImpl->Execute(host, port, request);
}

void HttpsClient::Stop() {
    write(commandFd, "q", 1);
}

void HttpsClient::Run() {
    netwServer->Run();
}
