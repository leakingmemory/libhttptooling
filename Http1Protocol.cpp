//
// Created by sigsegv on 3/8/25.
//

#include "Http1Protocol.h"

constexpr bool TestHttp1RequestLine(const std::string &ln, const std::string &expectedMethod, const std::string &expectedPath, const std::string &expectedVersion) {
    Http1RequestLine req{ln};
    if (expectedMethod != req.GetMethod()) {
        return false;
    }
    if (expectedPath != req.GetPath()) {
        return false;
    }
    return expectedVersion == req.GetVersion();
}

static_assert(TestHttp1RequestLine("GET /", "GET", "/", ""));
static_assert(TestHttp1RequestLine(" GET  /  HTTP/1.1", "GET", "/", "HTTP/1.1"));
static_assert(TestHttp1RequestLine("GET / HTTP/1.1", "GET", "/", "HTTP/1.1"));
static_assert(TestHttp1RequestLine("POST /index.html HTTP/1.1", "POST", "/index.html", "HTTP/1.1"));

static_assert(Http1RequestLine("GET", "/", "HTTP/1.1").operator std::string() == "GET / HTTP/1.1");
static_assert(Http1RequestLine("POST", "/index.html", "HTTP/1.1").operator std::string() == "POST /index.html HTTP/1.1");

constexpr bool TestHttp1Header(const std::string &ln, const std::string &expectedHeader, const std::string &expectedValue) {
    Http1HeaderLine hdr{ln};
    if (expectedHeader != hdr.GetHeader()) {
        return false;
    }
    return expectedValue == hdr.GetValue();
}

static_assert(TestHttp1Header("Content-Type: text/html", "Content-Type", "text/html"));
static_assert(Http1HeaderLine("Content-Type", "text/html").operator std::string() == "Content-Type: text/html");

static_assert(Http1Request({"GET", "/", "HTTP/1.1"}, {{"Accept", "text/html"}}).operator std::string() == "GET / HTTP/1.1\r\nAccept: text/html\r\n\r\n");
static_assert(!Http1RequestParser("").IsValid());
static_assert(Http1RequestParser("").IsTruncatedValid());
static_assert(!Http1RequestParser("GET /").IsValid());
static_assert(Http1RequestParser("GET /").IsTruncatedValid());
static_assert(Http1RequestParser("GET /\r\n").IsValid());
static_assert(Http1RequestParser("GET /\r\n").GetParsedInputCharacters() == std::string("GET /\r\n").size());
static_assert(!Http1RequestParser("GET / HTTP/1.1").IsValid());
static_assert(Http1RequestParser("GET / HTTP/1.1").IsTruncatedValid());
static_assert(!Http1RequestParser("GET / HTTP/1.1\r\n").IsValid());
static_assert(Http1RequestParser("GET / HTTP/1.1\r\n").IsTruncatedValid());
static_assert(!Http1RequestParser("GET / HTTP/1.1\r\nAccept: text/html").IsValid());
static_assert(Http1RequestParser("GET / HTTP/1.1\r\nAccept: text/html").IsTruncatedValid());
static_assert(!Http1RequestParser("GET / HTTP/1.1\r\nAccept: text/html\r\n").IsValid());
static_assert(Http1RequestParser("GET / HTTP/1.1\r\nAccept: text/html\r\n").IsTruncatedValid());
static_assert(Http1RequestParser("GET / HTTP/1.1\r\nAccept: text/html\r\n\r\n").IsValid());
static_assert(Http1RequestParser("GET / HTTP/1.1\r\nAccept: text/html\r\n\r\n").GetParsedInputCharacters() == std::string("GET / HTTP/1.1\r\nAccept: text/html\r\n\r\n").size());

static_assert(Http1Chunk("0\r\n\r\n", true).IsValid());
static_assert(Http1Chunk("0\r\n\r\n", true).GetConsumedBytes() == 5);
static_assert(Http1Chunk("0\r\n\r\n", true).GetChunk().empty());
static_assert(!Http1Chunk("0\r\n\r", true).IsValid());
static_assert(Http1Chunk("0\r\n\r", true).IsTruncated());
static_assert(Http1Chunk("1f\r\n012345678901234567890123456789s\r\n", true).IsValid());
static_assert(Http1Chunk("1f\r\n012345678901234567890123456789s\r\n", true).GetConsumedBytes() == std::string("1f\r\n012345678901234567890123456789s\r\n").size());
static_assert(Http1Chunk("1f\r\n012345678901234567890123456789s\r\n", true).GetChunk() == "012345678901234567890123456789s");
static_assert(Http1Chunk("", false).GetEncoded() == "0\r\n\r\n");
static_assert(Http1Chunk("012345678901234567890123456789s", false).GetEncoded() == "1f\r\n012345678901234567890123456789s\r\n");

static_assert(!Http1ResponseLine("OK").IsValid());
static_assert(Http1ResponseLine("HTTP/1.1 200 OK").IsValid());
static_assert(Http1ResponseLine("HTTP/1.1 200 OK").GetVersion() == "HTTP/1.1");
static_assert(Http1ResponseLine("HTTP/1.1 200 OK").GetCode() == 200);
static_assert(Http1ResponseLine("HTTP/1.1 200 OK").GetDescription() == "OK");
static_assert(Http1ResponseLine("HTTP/1.1", 200, "OK").operator std::string() == "HTTP/1.1 200 OK");

static_assert(Http1ResponseParser("HTTP/1.1 200 OK\r\nContent-Length: 13\r\n\r\n").IsValid());
static_assert(Http1ResponseParser("HTTP/1.1 200 OK\r\nContent-Length: 13\r\n\r\n").GetParsedInputCharacters() == std::string("HTTP/1.1 200 OK\r\nContent-Length: 13\r\n\r\n").size());
static_assert(Http1ResponseParser("HTTP/1.1 200 OK\r\nContent-Length: 13\r\n\r\n").operator Http1Response().GetResponseLine().GetVersion() == "HTTP/1.1");
static_assert(Http1ResponseParser("HTTP/1.1 200 OK\r\nContent-Length: 13\r\n\r\n").operator Http1Response().GetResponseLine().GetCode() == 200);
static_assert(Http1ResponseParser("HTTP/1.1 200 OK\r\nContent-Length: 13\r\n\r\n").operator Http1Response().GetResponseLine().GetDescription() == "OK");
static_assert(Http1ResponseParser("HTTP/1.1 200 OK\r\nContent-Length: 13\r\n\r\n").operator Http1Response().GetHeaderLines().size() == 1);
static_assert(Http1ResponseParser("HTTP/1.1 200 OK\r\nContent-Length: 13\r\n\r\n").operator Http1Response().GetHeaderLines()[0].GetHeader() == "Content-Length");
static_assert(Http1ResponseParser("HTTP/1.1 200 OK\r\nContent-Length: 13\r\n\r\n").operator Http1Response().GetHeaderLines()[0].GetValue() == "13");
static_assert(Http1ResponseParser("HTTP/1.1 200 OK\nContent-Length: 13\n\n").IsValid());
static_assert(!Http1ResponseParser("HTTP/1.1 200 OK\r\nContent-Length: 13\r\n\r").IsValid());
static_assert(Http1ResponseParser("HTTP/1.1 200 OK\r\nContent-Length: 13\r\n\r").IsTruncated());
static_assert(!Http1ResponseParser("HTTP/1.1 200 OK\r\nContent-Length: 13\r\n").IsValid());
static_assert(Http1ResponseParser("HTTP/1.1 200 OK\r\nContent-Length: 13\r\n").IsTruncated());
static_assert(!Http1ResponseParser("HTTP/1.1 200 OK\r\nConte").IsValid());
static_assert(Http1ResponseParser("HTTP/1.1 200 OK\r\nConte").IsTruncated());
static_assert(!Http1ResponseParser("HTTP/1.1 200 OK").IsValid());
static_assert(Http1ResponseParser("HTTP/1.1 200 OK").IsTruncated());
static_assert(!Http1ResponseParser("HTTP/1.1").IsValid());
static_assert(Http1ResponseParser("HTTP/1.1").IsTruncated());
static_assert(!Http1ResponseParser("").IsValid());
static_assert(Http1ResponseParser("").IsTruncated());

static_assert(Http1Response({"HTTP/1.1", 200, "OK"}, {{"Content-Length", "13"}}).operator std::string() == "HTTP/1.1 200 OK\r\nContent-Length: 13\r\n\r\n");
