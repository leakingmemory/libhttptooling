#include <thread>
#include <csignal>
#include "Poller.h"
#include "NetwServer.h"
#include "HttpServer.h"

extern "C" {
#include <unistd.h>
}

static int commandFd;

void signal_handler(int signal) {
    write(commandFd, "q", 1);
}

int main() {
    auto netwServer = NetwServer::Create(8080, std::make_shared<HttpServer>());
    commandFd = netwServer->GetCommandFd();
    std::signal(SIGTERM, signal_handler);
    netwServer->Run();
}
