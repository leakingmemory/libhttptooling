//
// Created by sigsegv on 3/2/25.
//

#ifndef LIBHTTPTOOLING_POLLER_H
#define LIBHTTPTOOLING_POLLER_H

#include <vector>
#include <memory>
#include <map>
#include <semaphore>
#include <mutex>
#include "include/task.h"

extern "C" {
    #include <poll.h>
};

enum class PollerResult {
    OK, TIMEOUT, ERROR
};

class Poller : public std::enable_shared_from_this<Poller> {
private:
    std::vector<struct pollfd> pollfds{};
    std::map<decltype(pollfds[0].fd),std::tuple<bool,bool,bool>> results{};
    std::vector<std::function<void ()>> runQueue{};
    std::counting_semaphore<16384> runQueueSemaphore{0};
    std::mutex runQueueMutex{};
    Poller() = default;
public:
    static std::shared_ptr<Poller> Create();
    void AddFd(int fd, bool read, bool write, bool err);
    void UpdateFd(int fd, bool read, bool write);
    void RemoveFd(int fd);
    void ClearFds();
    std::tuple<bool,bool,bool> GetResults(int fd);
    task<PollerResult> Poll(uint64_t timeoutMs);
    void Runner();
};


#endif //LIBHTTPTOOLING_POLLER_H
