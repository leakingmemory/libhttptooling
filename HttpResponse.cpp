//
// Created by sigsegv on 3/16/25.
//

#include "HttpResponse.h"


task<std::string> HttpResponse::ResponseBody() {
    std::string content{this->content};
    func_task<std::string> fnTask{[content] (const auto &fn) { fn(content); }};
    co_return co_await fnTask;
}