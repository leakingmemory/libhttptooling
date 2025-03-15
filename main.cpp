#include <thread>
#include <csignal>
#include "HttpServer.h"
#include "include/sync_coroutine.h"

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

int main() {
    server = HttpServer::Create(8080);
    FireAndForget<task<void>>([] () { return HttpServerLoop(); });
    std::signal(SIGTERM, signal_handler);
    server->Run();
    server = {};
}
