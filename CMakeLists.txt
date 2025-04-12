cmake_minimum_required(VERSION 3.31)
project(libhttptooling)

set(CMAKE_CXX_STANDARD 23)

#if (${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD")
#    add_compile_options(-I/usr/local/include -L/usr/local/lib)
#endif()

add_library(httptooling OBJECT
        include/sync_coroutine.h
        Poller.cpp
        Poller.h
        Fd.cpp
        Fd.h
        NetwServer.cpp
        NetwServer.h
        Http1Protocol.cpp
        Http1Protocol.h
        EchoServer.cpp
        EchoServer.h
        HttpServerImpl.cpp
        HttpServerImpl.h
        HttpHeaders.h
        HttpServer.cpp
        HttpServer.h
        HttpResponse.cpp
        HttpResponse.h
        HttpRequest.h
        HttpClientImpl.cpp
        HttpClientImpl.h
        HttpRequestImpl.cpp
        HttpRequestImpl.h
        HttpServerResponseContainer.h
        HttpServerConnectionHandler.h
        HttpClient.cpp
        HttpClient.h)

add_executable(ClientServerTest ClientServerTest.cpp)

target_link_libraries(ClientServerTest PRIVATE httptooling)
target_link_libraries(ClientServerTest PRIVATE -lpthread)

enable_testing()

add_test(ClientServerTest ClientServerTest)

#set_target_properties(httptooling PROPERTIES SOVERSION 1 VERSION 1.0.0)
#target_link_libraries(httptooling PRIVATE /usr/local/lib/libcoro.so)
