//
// Created by sigsegv on 3/1/25.
//

#ifndef LIBHTTPTOOLING_POLL_H
#define LIBHTTPTOOLING_POLL_H

#include <memory>
#include "include/task.h"

struct PollEvent {
    bool read : 1;
    bool write : 1;
    bool error : 1;
};

class Poll {
public:
    static std::shared_ptr<Poll> Create();
    virtual task<void> WaitFor(int fd, bool read, bool write, bool error);
};


#endif //LIBHTTPTOOLING_POLL_H
