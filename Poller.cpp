//
// Created by sigsegv on 3/2/25.
//

#include "Poller.h"
#include <csignal>

constexpr decltype(std::declval<struct pollfd>().events) noFlags = 0;
constexpr decltype(std::declval<struct pollfd>().events) readFlags = POLLIN | POLLRDNORM | POLLRDBAND | POLLPRI;
constexpr decltype(std::declval<struct pollfd>().events) writeFlags = POLLOUT | POLLWRNORM | POLLWRBAND;
constexpr decltype(std::declval<struct pollfd>().events) errFlagsRequest = POLLRDHUP;
constexpr decltype(std::declval<struct pollfd>().revents) errFlagsReport = POLLERR | POLLHUP | POLLRDHUP | POLLNVAL;

std::shared_ptr<Poller> Poller::Create() {
    std::shared_ptr<Poller> shptr{new Poller()};
    return shptr;
}

void Poller::AddFd(int fd, bool read, bool write, bool err) {
    decltype(std::declval<struct pollfd>().events) flags = static_cast<decltype(std::declval<struct pollfd>().events)>((read ? readFlags : noFlags) | (write ? writeFlags : noFlags) | (err ? errFlagsRequest : noFlags));
    struct pollfd pfd {
        .fd = fd,
        .events = flags,
        .revents = 0
    };
    pollfds.emplace_back(pfd);
}

void Poller::UpdateFd(int fd, bool read, bool write) {
    for (auto &pfd : pollfds) {
        if (pfd.fd == fd) {
            if (read) {
                pfd.events |= readFlags;
            } else {
                pfd.events &= ~readFlags;
            }
            if (write) {
                pfd.events |= writeFlags;
            } else {
                pfd.events &= ~writeFlags;
            }
            return;
        }
    }
}

void Poller::RemoveFd(int fd) {
    auto iterator = pollfds.begin();
    while (iterator != pollfds.end()) {
        const auto &pfd = *iterator;
        if (pfd.fd == fd) {
            iterator = pollfds.erase(iterator);
            return;
        }
        ++iterator;
    }
}

void Poller::ClearFds() {
    pollfds.clear();
}

std::tuple<bool, bool, bool> Poller::GetResults(int fd) {
    auto iterator = results.find(fd);
    if (iterator != results.end()) {
        return iterator->second;
    } else {
        return std::make_tuple<bool,bool,bool>(false, false, false);
    }
}

task<PollerResult> Poller::Poll(uint64_t timeoutMs) {
    func_task<int> pollSyscall{[this, timeoutMs] (const auto &callback) {
        {
            auto selfptr = shared_from_this();
            std::lock_guard lg{selfptr->runQueueMutex};
            selfptr->runQueue.emplace_back([selfptr, timeoutMs, callback]() mutable {
                sigset_t sigmask{};
                sigprocmask(0, NULL, &sigmask);
                uint64_t seconds = timeoutMs / 1000;
                if (seconds > std::numeric_limits<time_t>::max()) {
                    seconds = std::numeric_limits<time_t>::max();
                }
                uint64_t ns = timeoutMs % 1000;
                ns *= 1000000;
                struct timespec tm{.tv_sec = (time_t) seconds, .tv_nsec = (long) ns};
                selfptr->results.clear();
                auto err = ppoll(selfptr->pollfds.data(), selfptr->pollfds.size(), &tm, &sigmask);
                if (err > 0) {
                    for (const auto &fd : selfptr->pollfds) {
                        bool read = (fd.revents & readFlags) != 0;
                        bool write = (fd.revents & writeFlags) != 0;
                        bool err = (fd.revents & errFlagsReport) != 0;
                        if (read || write || err) {
                            auto tuple = std::make_tuple<bool,bool,bool>(read ? true : false, write ? true : false, err ? true : false);
                            selfptr->results.insert_or_assign(fd.fd, tuple);
                        }
                    }
                }
                selfptr = {};
                callback(err);
            });
            selfptr = {};
        }
        runQueueSemaphore.release();
    }};
    int err = co_await pollSyscall;
    if (err > 0) {
        co_return PollerResult::OK;
    } else if (err == 0) {
        co_return PollerResult::TIMEOUT;
    } else {
        co_return PollerResult::ERROR;
    }
}

void Poller::Runner() {
    runQueueSemaphore.acquire();
    std::vector<std::function<void ()>> runUnlocked{};
    {
        std::lock_guard lg{runQueueMutex};
        for (const auto &f: runQueue) {
            runUnlocked.emplace_back(f);
        }
        runQueue.clear();
    }
    for (const auto &f : runUnlocked) {
        f();
    }
}