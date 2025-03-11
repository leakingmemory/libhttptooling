//
// Created by sigsegv on 3/1/25.
//

#ifndef LIBHTTPTOOLING_TASK_H
#define LIBHTTPTOOLING_TASK_H

#include <functional>
#include <coroutine>
#include <exception>
#include <atomic>

//#define DEBUG_LF_MAG 0xF1F21234

template <typename T> class func_task {
private:
    std::function<void (const std::function<void (T)> &)> funcAwait;
    std::shared_ptr<T> value_;
#ifdef DEBUG_LF_MAG
    uint32_t magic{DEBUG_LF_MAG};
#endif
public:
    constexpr func_task() : funcAwait(), value_() {
    }
    constexpr func_task(const std::function<void (const std::function<void (T)> &)> &funcAwait) : funcAwait(funcAwait) {
        value_ = std::make_shared<T>();
    }
    constexpr func_task(std::function<void (const std::function<void (T)> &)> &&funcAwait) : funcAwait(std::move(funcAwait)) {
        value_ = std::make_shared<T>();
    }
    func_task(const func_task &) = delete;
    constexpr func_task(func_task &&) = default;
    func_task &operator = (const func_task &) = delete;
    constexpr func_task &operator = (func_task &&) = default;
    constexpr ~func_task() {
#ifdef DEBUG_LF_MAG
        if (magic != DEBUG_LF_MAG) {
            std::terminate();
        }
        magic = 0;
#endif
    }
    constexpr bool await_ready() const noexcept {
        return false;
    }
    constexpr void await_suspend(std::coroutine_handle<> h) {
#ifdef DEBUG_LF_MAG
        if (magic != DEBUG_LF_MAG) {
            std::terminate();
        }
#endif
        auto value = value_;
        funcAwait([value, h](T rv) {
            *value = rv;
            h.resume();
        });
    }
    constexpr T await_resume() const {
#ifdef DEBUG_LF_MAG
        if (magic != DEBUG_LF_MAG) {
            std::terminate();
        }
#endif
        return *value_;
    }
};

template <> class func_task<void> {
private:
    std::function<void (const std::function<void ()> &)> funcAwait;
#ifdef DEBUG_LF_MAG
    uint32_t magic{DEBUG_LF_MAG};
#endif
public:
    constexpr func_task(const std::function<void (const std::function<void ()> &)> &funcAwait) : funcAwait(funcAwait) {}
    constexpr func_task(std::function<void (const std::function<void ()> &)> &&funcAwait) : funcAwait(std::move(funcAwait)) {}
    constexpr ~func_task() {
#ifdef DEBUG_LF_MAG
        if (magic != DEBUG_LF_MAG) {
            std::terminate();
        }
        magic = 0;
#endif
    }
    constexpr bool await_ready() const noexcept {
        return false;
    }
    void await_suspend(std::coroutine_handle<> h) {
#ifdef DEBUG_LF_MAG
        if (magic != DEBUG_LF_MAG) {
            std::terminate();
        }
#endif
        funcAwait([h]() {
            h.resume();
        });
    }
    constexpr void await_resume() const {
#ifdef DEBUG_LF_MAG
        if (magic != DEBUG_LF_MAG) {
            std::terminate();
        }
#endif
    }
};

template <typename T> struct task {
    struct promise_type {
        std::function<void ()> callback_;
        std::atomic_bool callCallback{false};
        std::atomic_bool returnedCall{false};
        T value_;
        std::suspend_never initial_suspend() noexcept {
            return {};
        }
        std::suspend_never final_suspend() noexcept {
            return {};
        }
        task<T> get_return_object() {
            return {std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        void unhandled_exception() {
            std::terminate();
        }
        void return_value(T rv) {
            value_ = rv;
            returnedCall.store(true);
            if (callCallback) {
                callback_();
            }
        }
    };
private:
    std::coroutine_handle<promise_type> handle;
    std::atomic_bool resumed{false};
    bool hasHandle{false};
#ifdef DEBUG_LF_MAG
    uint32_t magic{DEBUG_LF_MAG};
#endif
public:
    task(std::coroutine_handle<promise_type> h) : handle(h), hasHandle(true) {
    }
    task(task &&mv) : handle(std::move(mv.handle)), hasHandle(mv.hasHandle) {
        mv.hasHandle = false;
    }
    ~task() {
#ifdef DEBUG_LF_MAG
        if (magic != DEBUG_LF_MAG) {
            std::terminate();
        }
        magic = 0;
#endif
        hasHandle = false;
    }

    bool await_ready() {
        return false;
    }
    void await_suspend(std::coroutine_handle<> h) noexcept {
#ifdef DEBUG_LF_MAG
        if (magic != DEBUG_LF_MAG) {
            std::terminate();
        }
#endif
        auto &promise = handle.promise();
        resumed.store(false);
        promise.callback_ = [this, h] () {
#ifdef DEBUG_LF_MAG
            if (magic != DEBUG_LF_MAG) {
                std::terminate();
            }
#endif
            bool expect{false};
            if (resumed.compare_exchange_strong(expect, true)) {
                h.resume();
            }
        };
        if (promise.returnedCall.load()) {
            bool expect{false};
            if (resumed.compare_exchange_strong(expect, true)) {
                h.resume();
            }
        } else {
            promise.callCallback.store(true);
            if (promise.returnedCall.load()) {
                bool expect{false};
                if (resumed.compare_exchange_strong(expect, true)) {
                    h.resume();
                }
            }
        }
    }
    T await_resume() noexcept {
#ifdef DEBUG_LF_MAG
        if (magic != DEBUG_LF_MAG) {
            std::terminate();
        }
#endif
        return handle.promise().value_;
    }
};

template <> struct task<void> {
    struct promise_type {
        std::function<void ()> callback_;
        std::atomic_bool callCallback{false};
        std::atomic_bool returnedCall{false};
        std::suspend_never initial_suspend() noexcept {
            return {};
        }
        std::suspend_never final_suspend() noexcept {
            return {};
        }
        task<void> get_return_object() {
            return {std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        void unhandled_exception() {
            std::terminate();
        }
        void return_void() {
            returnedCall.store(true);
            if (callCallback) {
                callback_();
            }
        }
    };
private:
    std::coroutine_handle<promise_type> handle;
    std::atomic_bool resumed{false};
    bool handlePresent{false};
#ifdef DEBUG_LF_MAG
    uint32_t magic{DEBUG_LF_MAG};
#endif
public:
    task(std::coroutine_handle<promise_type> h) : handle(h), handlePresent(true) {
    }
    task(task &&mv) : handle(std::move(mv.handle)), handlePresent(mv.handlePresent) {
        mv.handlePresent = false;
    }
    task &operator =(task &&mv) {
        if (this == &mv) {
            return *this;
        }
        handle = std::move(mv.handle);
        handlePresent = mv.handlePresent;
        mv.handlePresent = false;
        return *this;
    }
    task() {
    }
    ~task() {
#ifdef DEBUG_LF_MAG
        if (magic != DEBUG_LF_MAG) {
            std::terminate();
        }
        magic = 0;
#endif
    }

    bool await_ready() {
        return false;
    }
    void await_suspend(std::coroutine_handle<> h) noexcept {
#ifdef DEBUG_LF_MAG
        if (magic != DEBUG_LF_MAG) {
            std::terminate();
        }
#endif
        auto &promise = handle.promise();
        resumed.store(false);
        promise.callback_ = [this, h] () {
#ifdef DEBUG_LF_MAG
            if (magic != DEBUG_LF_MAG) {
                std::terminate();
            }
#endif
            bool expect{false};
            if (resumed.compare_exchange_strong(expect, true)) {
                h.resume();
            }
        };
        if (promise.returnedCall.load()) {
            bool expect{false};
            if (resumed.compare_exchange_strong(expect, true)) {
                h.resume();
            }
        } else {
            promise.callCallback.store(true);
            if (promise.returnedCall.load()) {
                bool expect{false};
                if (resumed.compare_exchange_strong(expect, true)) {
                    h.resume();
                }
            }
        }
    }
    void await_resume() noexcept {
    }
};

#endif //LIBHTTPTOOLING_TASK_H
