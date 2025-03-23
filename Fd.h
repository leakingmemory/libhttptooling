//
// Created by sigsegv on 3/2/25.
//

#ifndef LIBHTTPTOOLING_FD_H
#define LIBHTTPTOOLING_FD_H


#include <tuple>
#include <exception>
#include <string>

class FdException : public std::exception {
private:
    std::string errorMsg;
public:
    FdException() : errorMsg("Input output error") {}
    FdException(const std::string &errorMsg) : errorMsg(errorMsg) {}
    const char * what() const noexcept override;
};

class Fd {
private:
    int fd;
public:
    Fd() : fd(-1) {}
protected:
    Fd(int fd);
public:
    bool IsValid() const {
        return fd >= 0;
    }
    Fd(const Fd &) = delete;
    Fd &operator =(const Fd &) = delete;
    Fd(Fd &&mv) : fd(mv.fd) {
        mv.fd = -1;
    }
    Fd &operator =(Fd &&mv);
    ~Fd();
    static std::tuple<Fd,Fd> Pipe(bool closeOnExec = true, bool nonblock = false);
    static Fd InetSocket();
    void BindListen(int port);
    void Listen(int backlog);
    void Connect(const void *ipaddr_norder, size_t ipaddr_size, int port);
    void SetNonblocking();
    Fd Accept();
protected:
    size_t Write(const void *ptr, size_t size) const;
    size_t Read(void *ptr, size_t size) const;
    template <class T> constexpr size_t FinalIoSize(T input, T::size_type offset, T::size_type size) const {
        if (offset >= input.size()) {
            return 0;
        }
        typename T::size_type available = input.size() - offset;
        auto result = size;
        if (available < result) {
            result = available;
        }
        if (result > std::numeric_limits<size_t>::max()) {
            result = std::numeric_limits<size_t>::max();
        }
        if (result > std::numeric_limits<int>::max()) {
            result = std::numeric_limits<int>::max();
        }
        if (result < 0) {
            result = 0;
        }
        return result;
    }
public:
    template <class T> size_t Write(const T &input, T::size_type offset = 0, T::size_type size = std::numeric_limits<typename T::size_type>::max()) const {
        return Write(input.data() + offset, FinalIoSize(input, offset, size));
    }
    template <class T> size_t Read(T &output, T::size_type offset = 0, T::size_type size = std::numeric_limits<typename T::size_type>::max()) const {
        return Read(output.data() + offset, FinalIoSize(output, offset, size));
    }
    operator decltype(fd) ()const  {
        return fd;
    }
};


#endif //LIBHTTPTOOLING_FD_H
