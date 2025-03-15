//
// Created by sigsegv on 3/15/25.
//

#ifndef LIBHTTPTOOLING_HTTPHEADERS_H
#define LIBHTTPTOOLING_HTTPHEADERS_H

#include <type_traits>
#include <concepts>
#include <string>
#include <algorithm>

template <class T> concept HttpHeadClass = requires (T t){
    { t.GetHeader().begin()->GetHeader() } -> std::convertible_to<std::string>;
    { t.GetHeader().begin()->GetValue() } -> std::convertible_to<std::string>;
    { t.GetHeader().end()->GetHeader() } -> std::convertible_to<std::string>;
    { t.GetHeader().end()->GetValue() } -> std::convertible_to<std::string>;
};

template <HttpHeadClass T> class HttpHeaderValues {
private:
    T headObj;
public:
    constexpr HttpHeaderValues(const T &headObj) : headObj(headObj) {
        ContentLength.headObj = &(this->headObj);
    }

private:
    template <typename I> constexpr static I AsInt(const std::string &str) {
        I val = 0;
        for (auto ch : str) {
            if (ch < '0' || ch > '9') {
                return 0;
            }
            val *= 10;
            val += ch - '0';
        }
        return val;
    }
public:

    struct {
        friend HttpHeaderValues;
    private:
        const T *headObj{nullptr};
    public:
        constexpr operator std::string () const {
            for (const auto &header : headObj->GetHeader()) {
                auto name = header.GetHeader();
                std::transform(name.cbegin(), name.cend(), name.begin(), [] (char ch) { return std::tolower(ch); });
                if (name == "content-length") {
                    return header.GetValue();
                }
            }
            return "0";
        }
        constexpr operator size_t () const {
            return AsInt<size_t>(operator std::string());
        }
    } ContentLength;
};

#endif //LIBHTTPTOOLING_HTTPHEADERS_H
