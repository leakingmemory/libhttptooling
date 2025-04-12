//
// Created by sigsegv on 3/16/25.
//

#include "HttpResponse.h"


task<ResponseBodyResult> HttpResponse::ResponseBody() {
    std::string content{this->content};
    func_task<ResponseBodyResult> fnTask{[content] (const auto &fn) { fn({.body = content, .success = true}); }};
    co_return co_await fnTask;
}