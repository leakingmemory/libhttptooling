#include <iostream>
#include <thread>
#include <csignal>
#include "include/sync_coroutine.h"
#include "include/task.h"
#include "Poller.h"
#include "NetwServer.h"
extern "C" {
#include <unistd.h>
}

static int commandFd;

void signal_handler(int signal) {
    write(commandFd, "q", 1);
}

int main() {
    auto netwServer = NetwServer::Create(8080);
    commandFd = netwServer->GetCommandFd();
    std::signal(SIGTERM, signal_handler);
    netwServer->Run();
    /*auto poller = Poller::Create();
    FireAndForget<task<void>>([poller] () -> task<void> { return PollLoop(poller); });
    while (true) {
        std::cout << "Cycle\n";
        poller->Runner();
    }*/
}
