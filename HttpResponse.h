//
// Created by sigsegv on 3/15/25.
//

#ifndef LIBHTTPTOOLING_HTTPRESPONSE_H
#define LIBHTTPTOOLING_HTTPRESPONSE_H

#include <string>
#include "include/task.h"

class HttpResponse {
private:
    std::string content;
    std::string contentType;
    std::string description;
    int code;
public:
    constexpr HttpResponse(int code, const std::string &description) : code(code), description(description) {}
    constexpr HttpResponse(int code, std::string &&description) : code(code), description(std::move(description)) {}
    constexpr std::string GetContent() const {
        return content;
    }
    constexpr size_t GetContentLength() const {
        return content.size();
    }
    constexpr std::string GetContentType() const {
        return contentType;
    }
    constexpr std::string GetDescription() const {
        return description;
    }
    constexpr int GetCode() const {
        return code;
    }
    constexpr void SetContent(const std::string &content, const std::string &contentType){
        this->content = content;
        this->contentType = contentType;
    }
    constexpr void SetContent(std::string &&content, std::string &&contentType){
        this->content = std::move(content);
        this->contentType = std::move(contentType);
    }
    virtual task<std::string> ResponseBody();
};

#endif //LIBHTTPTOOLING_HTTPRESPONSE_H
