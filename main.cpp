//
// Created by sigsegv on 4/12/25.
//

#include <iostream>
#include "HttpsClient.h"
#include "include/sync_coroutine.h"

task<void> HttpClientStuff(const std::shared_ptr<HttpsClient> &clientIn) {
    std::shared_ptr<HttpsClient> client = clientIn;
    auto request = client->Request("GET", "/test");
    std::cout << "Exec req\n";
    auto responseExpected = co_await client->Execute("appredirect.radiotube.org", 80, request);
    if (responseExpected.has_value()) {
        auto response = responseExpected.value();
        std::cout << "Client response " << response->GetCode() << " " << response->GetDescription() << "\n";
        auto responseContent = co_await response->ResponseBody();
        std::cout << "Client response body: " << responseContent.body
                  << (responseContent.success ? " (successful)" : " (error)") << "\n";
    } else {
        std::cerr << "FdException: " << responseExpected.error().what() << "\n";
        client->Stop();
        std::cout << "Client stop requested\n";
    }
}

int main() {
    auto client = HttpsClient::Create();
    FireAndForget<task<void>>([client] () { return HttpClientStuff(client); });
    std::cout << "Client starts\n";
    client->Run();
    std::cout << "Client stopped\n";
    client = {};
}