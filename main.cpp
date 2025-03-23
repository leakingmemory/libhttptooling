#include <thread>
#include <csignal>
#include "HttpServer.h"
#include "HttpClient.h"
#include "include/sync_coroutine.h"
#include "Fd.h"

static std::shared_ptr<HttpServer> server{};

void signal_handler(int signal) {
    server->Stop();
}

#include <iostream>
task<void> HttpServerLoop() {
    while (true) {
        auto req = co_await server->NextRequest();
        std::cout << req->GetMethod() << " " << req->GetPath() << "\n";
        if (req->GetMethod() == "POST") {
            auto response = std::make_shared<HttpResponse>(200, "OK");
            auto reqBody = co_await req->RequestBody();
            response->SetContent(reqBody, "text/plain");
            std::cout << "Responding to body of size " << reqBody.size() << "\n";
            req->Respond(response);
        } else {
            auto response = std::make_shared<HttpResponse>(404, "Not found");
            response->SetContent("Not found\r\n", "text/plain");
            req->Respond(response);
        }
    }
}

task<void> HttpClientStuff(const std::shared_ptr<HttpClient> &clientIn) {
    std::shared_ptr<HttpClient> client{clientIn};
    auto request = client->Request("POST", "/test");
    request->SetContent("hello", "text/plain");
    std::cout << "Client request /test\n";
    auto responseExpected = co_await client->Execute("127.0.0.1", 8080, request);
    if (responseExpected.has_value()) {
        auto response = responseExpected.value();
        std::cout << "Client response " << response->GetCode() << " " << response->GetDescription() << "\n";
        auto responseContent = co_await response->ResponseBody();
        std::cout << "Client response body: " << responseContent << "\n";
        client->Stop();
        std::cout << "Client stop requested\n";
    } else {
        std::cerr << "FdException: " << responseExpected.error().what() << "\n";
        client->Stop();
        std::cout << "Client stop requested\n";
    }
}

int main() {
    server = HttpServer::Create(8080);
    FireAndForget<task<void>>([] () { return HttpServerLoop(); });
    std::signal(SIGTERM, signal_handler);
    std::thread clientThread{[] () {
        auto client = HttpClient::Create();
        FireAndForget<task<void>>([client] () { return HttpClientStuff(client); });
        std::cout << "Client starts\n";
        client->Run();
        std::cout << "Client stopped\n";
    }};
    server->Run();
    server = {};
    clientThread.join();
}
