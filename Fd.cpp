//
// Created by sigsegv on 3/2/25.
//

#include "Fd.h"
extern "C" {
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
};

const char *FdException::what() const noexcept {
    return "File descriptor operation faled";
}

Fd::Fd(int fd) : fd(fd) {}

Fd &Fd::operator =(Fd &&mv) {
    if (this == &mv) {
        return *this;
    }
    if (fd >= 0) {
        close(fd);
    }
    fd = mv.fd;
    mv.fd = -1;
    return *this;
}
Fd::~Fd() {
    if (fd >= 0) {
        close(fd);
    }
}

std::tuple<Fd,Fd> Fd::Pipe(bool closeOnExec, bool nonblock) {
    int flags{0};
    if (closeOnExec) {
        flags |= O_CLOEXEC;
    }
    if (nonblock) {
        flags |= O_NONBLOCK;
    }
    int fds[2];
    if (pipe2(fds, flags) != 0) {
        throw FdException();
    }
    return std::make_tuple<Fd,Fd>(Fd(fds[0]), Fd(fds[1]));
}

Fd Fd::InetSocket() {
    auto fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        throw FdException();
    }
    return {fd};
}

void Fd::BindListen(int port) {
    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = htons(port);
    if (bind(fd, (struct sockaddr *) &sin, sizeof(sin)) < 0) {
        throw FdException();
    }
}

void Fd::Listen(int backlog) {
    if (listen(fd, backlog) != 0) {
        throw FdException();
    }
}

void Fd::SetNonblocking() {
    auto flags = fcntl(fd, F_GETFL);
    if (flags == -1) {
        throw FdException();
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        throw FdException();
    }
}

Fd Fd::Accept() {
    int cfd = accept(fd, NULL, NULL);
    if (cfd < 0) {
        return {};
    }
    return {cfd};
}

size_t Fd::Write(const void *ptr, size_t size) {
    if (size <= 0) {
        return 0;
    }
    auto res = write(fd, ptr, size);
    if (res >= 0) {
        return res;
    } else {
        if (errno == EAGAIN) {
            return 0;
        }
        throw FdException();
    }
}

size_t Fd::Read(void *ptr, size_t size) {
    if (size <= 0) {
        return 0;
    }
    auto res = read(fd, ptr, size);
    if (res >= 0) {
        return res;
    } else {
        if (errno == EAGAIN) {
            return 0;
        }
        throw FdException();
    }
}
