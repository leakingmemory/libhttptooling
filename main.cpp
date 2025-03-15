#include <thread>
#include <csignal>
#include "Poller.h"
#include "NetwServer.h"
#include "HttpServer.h"
#include "include/sync_coroutine.h"

extern "C" {
#include <unistd.h>
}

static int commandFd;

void signal_handler(int signal) {
    write(commandFd, "q", 1);
}

#include <iostream>
task<void> HttpServerLoop(const std::shared_ptr<HttpServer> &serverIn) {
    std::shared_ptr<HttpServer> server{serverIn};
    while (true) {
        auto req = co_await server->NextRequest();
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
    auto server = std::make_shared<HttpServer>();
    FireAndForget<task<void>>([server] () { return HttpServerLoop(server); });
    auto netwServer = NetwServer::Create(8080, server);
    commandFd = netwServer->GetCommandFd();
    std::signal(SIGTERM, signal_handler);
    netwServer->Run();
}
