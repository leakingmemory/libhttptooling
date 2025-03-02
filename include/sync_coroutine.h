//
// Created by sigsegv on 3/1/25.
//

#ifndef LIBHTTPTOOLING_SYNC_COROUTINE_H
#define LIBHTTPTOOLING_SYNC_COROUTINE_H

#include <latch>
#include <coroutine>
#include <exception>
#include <functional>

template <typename T> struct completion_token {
    std::latch latch{1};
    T rv{};
};

template <typename T> struct co_interface {
    typedef co_interface promise_type;
    ~co_interface() {
    }
    std::suspend_never initial_suspend() {
        return {};
    }
    std::suspend_never final_suspend() noexcept {
        return {};
    }
    promise_type get_return_object() noexcept {
        return {};
    }
    void return_value(completion_token<T> *ct) {
        ct->latch.count_down();
    }
    void unhandled_exception() {
        std::terminate();
    }
};

template <class T, class RV> co_interface<RV> SyncRunner(completion_token<RV> *ct, T &task) {
    ct->rv = co_await task;
    co_return ct;
}

template <class T> T FireAndForget(const std::function<T ()> &taskIncoming) {
    T *task = new T();
    *task = taskIncoming();
    co_await *task;
    delete task;
    co_return;
}

template <class RV, class T> RV RunSync(T &task) {
    completion_token<RV> ct{};
    SyncRunner(&ct, task);
    ct.latch.wait();
    return ct.rv;
}

template <class RV, class T> RV RunSync(T &&mv) {
    T task{std::move(mv)};
    completion_token<RV> ct{};
    SyncRunner(&ct, task);
    ct.latch.wait();
    return ct.rv;
}

#endif //LIBHTTPTOOLING_SYNC_COROUTINE_H
