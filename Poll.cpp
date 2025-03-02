//
// Created by sigsegv on 3/1/25.
//

#include "Poll.h"

task<void> Poll::WaitFor(int fd, bool read, bool write, bool error) {
    co_return;
}

class PollImpl : public Poll {
public:
    task<void> WaitFor(int fd, bool read, bool write, bool error) override;
};

std::shared_ptr<Poll> Poll::Create() {
    return std::make_shared<PollImpl>();
}

task<void> PollImpl::WaitFor(int fd, bool read, bool write, bool error) {
    co_return;
}
