//
// Created by sigsegv on 4/12/25.
//

#include "HttpsClientImpl.h"

class HttpsClientConnectionHandler : public NetwConnectionHandler, public std::enable_shared_from_this<HttpsClientConnectionHandler> {
private:
    std::function<void(const std::string &)> output;
    std::function<void()> close;
    std::shared_ptr<NetwProtocolHandler> protocolHandler{};
    NetwConnectionHandler *handler{nullptr};
public:
    HttpsClientConnectionHandler(const std::function<void(const std::string &)> &output, const std::function<void()> &close) : output(output), close(close) {}
    HttpsClientConnectionHandler(const HttpsClientConnectionHandler &) = delete;
    HttpsClientConnectionHandler(HttpsClientConnectionHandler &&) = delete;
    HttpsClientConnectionHandler &operator =(const HttpsClientConnectionHandler &) = delete;
    HttpsClientConnectionHandler &operator =(HttpsClientConnectionHandler &&) = delete;
    ~HttpsClientConnectionHandler();
    void Init(const std::shared_ptr<NetwProtocolHandler> &upstream);
    void SetupConnection(const std::function<void(NetwConnectionHandler *)> &callback);
    void Write(const std::string &);
    void Close();
    size_t AcceptInput(const std::string &) override;
    void EndOfConnection() override;
};

HttpsClientConnectionHandler::~HttpsClientConnectionHandler() {
    if (handler != nullptr) {
        protocolHandler->Release(handler);
        handler = nullptr;
        protocolHandler = {};
    }
}

void HttpsClientConnectionHandler::Init(const std::shared_ptr<NetwProtocolHandler> &upstream) {
    std::shared_ptr<HttpsClientConnectionHandler> shptr = shared_from_this();
    std::weak_ptr<HttpsClientConnectionHandler> wkptr{shptr};
    protocolHandler = upstream;
    handler = protocolHandler->Create([wkptr] (const std::string &output) {
        auto shptr = wkptr.lock();
        if (shptr) {
            shptr->Write(output);
        }
    }, [wkptr] () {
        auto shptr = wkptr.lock();
        if (shptr) {
            shptr->Close();
        }
    });
}

void HttpsClientConnectionHandler::SetupConnection(const std::function<void(NetwConnectionHandler *)> &callback) {
    callback(handler);
}

void HttpsClientConnectionHandler::Write(const std::string &buf) {
    output(buf);
}

void HttpsClientConnectionHandler::Close() {
    close();
}

size_t HttpsClientConnectionHandler::AcceptInput(const std::string &input) {
    if (handler != nullptr) {
        return handler->AcceptInput(input);
    } else {
        return input.size();
    }
}

void HttpsClientConnectionHandler::EndOfConnection() {
    if (handler != nullptr) {
        handler->EndOfConnection();
    }
}

class HttpsClientConnectionHandlerProxy : public NetwConnectionHandler {
private:
    std::shared_ptr<HttpsClientConnectionHandler> handler;
public:
    HttpsClientConnectionHandlerProxy(const std::shared_ptr<NetwProtocolHandler> &upstream, const std::function<void(const std::string &)> &output, const std::function<void()> &close) : handler(std::make_shared<HttpsClientConnectionHandler>(output, close)) {
        handler->Init(upstream);
    }
    size_t AcceptInput(const std::string &) override;
    void EndOfConnection() override;
    std::shared_ptr<HttpsClientConnectionHandler> GetHandler() const {
        return handler;
    }
};

size_t HttpsClientConnectionHandlerProxy::AcceptInput(const std::string &input) {
    return handler->AcceptInput(input);
}

void HttpsClientConnectionHandlerProxy::EndOfConnection() {
    handler->EndOfConnection();
}

NetwConnectionHandler *
HttpsClientImpl::Create(const std::function<void(const std::string &)> &output, const std::function<void()> &close) {
    return new HttpsClientConnectionHandlerProxy(upstreamHandler, output, close);
}

void HttpsClientImpl::Release(NetwConnectionHandler *handler) {
    delete handler;
}

void HttpsClientImpl::SetAssociatedNetwServer(const std::weak_ptr<NetwServerInterface> &netwServer) {
    this->netwServer = netwServer;
}

void HttpsClientImpl::Connect(const void *ipaddr_norder, size_t ipaddr_len, int port, const std::string &requestData,
                              const std::function<void(NetwConnectionHandler *)> &callback) {
    auto netwServer = this->netwServer.lock();
    if (!netwServer) {
        callback(nullptr);
        return;
    }
    netwServer->Connect(ipaddr_norder, ipaddr_len, port, requestData, [callback] (NetwConnectionHandler *rawHandler) {
        HttpsClientConnectionHandlerProxy *handlerProxy = dynamic_cast<HttpsClientConnectionHandlerProxy *>(rawHandler);
        if (handlerProxy == nullptr) {
            throw std::exception();
        }
        auto handler = handlerProxy->GetHandler();
        handler->SetupConnection(callback);
    });
}
