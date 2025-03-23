//
// Created by sigsegv on 3/8/25.
//

#ifndef LIBHTTPTOOLING_HTTP1PROTOCOL_H
#define LIBHTTPTOOLING_HTTP1PROTOCOL_H

#include <string>
#include <vector>

class Http1RequestLine {
private:
    std::string method;
    std::string path;
    std::string version;
public:
    static constexpr bool IsSpace(char ch) {
        return ch == ' ' || ch == '\t';
    }
    constexpr Http1RequestLine() : method(), path(), version() {}
    constexpr Http1RequestLine(const std::string &method, const std::string &path, const std::string &version) :
        method(method), path(path), version(version) {}
    constexpr Http1RequestLine(std::string &&method, std::string &&path, std::string &&version) :
            method(std::move(method)), path(std::move(path)), version(std::move(version)) {}
    constexpr Http1RequestLine(const std::string &input) : method(input), path(), version() {
        {
            decltype(method.size()) i = 0;
            while (i < method.size() && IsSpace(method[i])) {
                ++i;
            }
            if (i > 0) {
                method.erase(0, i);
            }
        }
        for (decltype(method.size()) i = 0; i < method.size(); i++) {
            if (IsSpace(method[i])) {
                auto methodLength = i;
                for (i++; i < method.size(); i++) {
                    if (!IsSpace(method[i])) {
                        path = method.substr(i);
                        break;
                    }
                }
                method.resize(methodLength);
                break;
            }
        }
        for (decltype(path.size()) i = 0; i < path.size(); i++) {
            if (IsSpace(path[i])) {
                auto pathLength = i;
                for (i++; i < path.size(); i++) {
                    if (!IsSpace(path[i])) {
                        version = path.substr(i);
                        break;
                    }
                }
                path.resize(pathLength);
                break;
            }
        }
    }
    constexpr std::string GetMethod() const {
        return method;
    }
    constexpr std::string GetPath() const {
        return path;
    }
    constexpr std::string GetVersion() const {
        return version;
    }
    constexpr operator std::string() const {
        if (method.empty() || path.empty()) {
            return {};
        }
        std::string output{method};
        output.append(" ");
        output.append(path);
        if (!version.empty()) {
            output.append(" ");
            output.append(version);
        }
        return output;
    }
};

class Http1HeaderLine {
private:
    std::string header;
    std::string value;
public:
    static constexpr bool IsSpace(char ch) {
        return ch == ' ' || ch == '\t';
    }
    constexpr Http1HeaderLine() : header(), value() {}
    constexpr Http1HeaderLine(const std::string &header, const std::string &value) : header(header), value(value) {}
    constexpr Http1HeaderLine(std::string &&header, std::string &&value) : header(std::move(header)), value(std::move(value)) {}
    constexpr Http1HeaderLine(const std::string &input) : header(input), value() {
        {
            decltype(header.size()) i = 0;
            while (i < header.size() && IsSpace(header[i])) {
                ++i;
            }
            if (i > 0) {
                header.erase(0, i);
            }
        }
        for (decltype(header.size()) i = 0; i < header.size(); i++) {
            if (header[i] == ':') {
                auto headerLength = i;
                for (i++; i < header.size(); i++) {
                    if (!IsSpace(header[i])) {
                        value = header.substr(i);
                        break;
                    }
                }
                header.resize(headerLength);
                break;
            }
        }
    }
    constexpr std::string GetHeader() const {
        return header;
    }
    constexpr std::string GetValue() const {
        return value;
    }
    constexpr operator std::string () const {
        if (header.empty()) {
            return {};
        }
        std::string output{header};
        output.append(": ");
        output.append(value);
        return output;
    }
};

class Http1Request {
private:
    Http1RequestLine requestLine{};
    std::vector<Http1HeaderLine> headerLines{};
public:
    constexpr Http1Request() = default;
    constexpr Http1Request(const Http1RequestLine &req, const std::vector<Http1HeaderLine> &hdr) : requestLine(req), headerLines(hdr) {
    }
    constexpr Http1RequestLine GetRequest() const {
        return requestLine;
    }
    constexpr std::vector<Http1HeaderLine> GetHeader() const {
        return headerLines;
    }
    constexpr operator std::string () const {
        std::string str{requestLine.operator std::string()};
        if (str.empty()) {
            return str;
        }
        str.append("\r\n");
        for (const auto &hdr : headerLines) {
            str.append(hdr.operator std::string());
            str.append("\r\n");
        }
        str.append("\r\n");
        return str;
    }
};

class Http1RequestParser {
private:
    Http1RequestLine requestLine{};
    std::vector<Http1HeaderLine> headerLines{};
    size_t parsedInputCharacters{0};
    bool validHttpRequest{false};
    bool truncatedHttpRequest{false};
public:
    constexpr bool IsLfOrCr(char ch) {
        return ch == '\n' || ch == '\r';
    }
    constexpr Http1RequestParser() = default;
    constexpr Http1RequestParser(const std::string &input) {
        if (input.empty()) {
            truncatedHttpRequest = true;
            return;
        }
        decltype(input.size()) i = 0;
        while (i < input.size() && !IsLfOrCr(input[i])) {
            ++i;
        }
        if (i == 0) {
            return;
        }
        if (i >= input.size()) {
            truncatedHttpRequest = true;
            return;
        }
        requestLine = Http1RequestLine(input.substr(0, i));
        if (requestLine.GetMethod().empty() || requestLine.GetPath().empty()) {
            return;
        }
        {
            auto prevCh = input[i];
            ++i;
            if (i < input.size() && IsLfOrCr(input[i]) && input[i] != prevCh) {
                ++i;
            }
        }
        if (requestLine.GetVersion().empty()) {
            auto method = requestLine.GetMethod();
            std::transform(method.cbegin(), method.cend(), method.begin(), [] (char ch) {
                switch (ch) {
                    case 'G':
                        return 'g';
                    case 'E':
                        return 'e';
                    case 'T':
                        return 't';
                    default:
                        return ch;
                }
            });
            validHttpRequest = method == "get";
            parsedInputCharacters = i;
            return;
        }
        while (i < input.size() && !IsLfOrCr(input[i])) {
            decltype(i) start = i;
            ++i;
            while (i < input.size() && !IsLfOrCr(input[i])) {
                ++i;
            }
            if (i >= input.size()) {
                truncatedHttpRequest = true;
                return;
            }
            Http1HeaderLine hdr{input.substr(start, i - start)};
            if (hdr.GetHeader().empty()) {
                return;
            }
            auto prevCh = input[i];
            ++i;
            if (i < input.size() && IsLfOrCr(input[i]) && prevCh != input[i]) {
                ++i;
            }
            headerLines.emplace_back(std::move(hdr));
        }
        if (i >= input.size()) {
            truncatedHttpRequest = true;
            return;
        }
        auto prevCh = input[i];
        ++i;
        if (IsLfOrCr(input[i]) && input[i] != prevCh) {
            ++i;
        }
        validHttpRequest = true;
        parsedInputCharacters = i;
    }
    constexpr size_t GetParsedInputCharacters() {
        return parsedInputCharacters;
    }
    constexpr bool IsValid() const {
        return validHttpRequest;
    }
    constexpr bool IsTruncatedValid() const {
        return truncatedHttpRequest;
    }
    constexpr operator Http1Request () const {
        return {requestLine, headerLines};
    }
};

class Http1Chunk {
private:
    std::string chunk{};
    bool isValid{false};
    bool isTruncated{false};
    size_t consumedBytes{0};
public:
    constexpr bool IsLfOrCr(char ch) {
        return ch == '\n' || ch == '\r';
    }
    constexpr Http1Chunk() = default;
    constexpr Http1Chunk(const std::string &input, bool encoded) : chunk(input), isValid(!encoded), isTruncated(false), consumedBytes(0) {
        if (encoded) {
            size_t sz{0};
            bool crLfSingleMode{false};
            for (decltype(chunk.size()) i = 0; i < chunk.size(); i++) {
                if (i > ((sizeof(size_t) * 2) - 1)) {
                    /* Risk of overflows due to excessive large chunk size, rejecting */
                    return;
                }
                if (IsLfOrCr(chunk[i])) {
                    if (i == 0) {
                        return;
                    }
                    auto prevCh = chunk[i];
                    ++i;
                    if (IsLfOrCr(chunk[i]) && chunk[i] != prevCh) {
                        ++i;
                    } else {
                        crLfSingleMode = true;
                    }
                    chunk.erase(0, i);
                    consumedBytes += i;
                    break;
                } else if (chunk[i] >= '0' && chunk[i] <= '9') {
                    sz = sz << 4;
                    sz += chunk[i] - '0';
                } else if (chunk[i] >= 'a' && chunk[i] <= 'f') {
                    sz = sz << 4;
                    sz += chunk[i] - 'a' + 10;
                } else if (chunk[i] >= 'A' && chunk[i] <= 'F') {
                    sz = sz << 4;
                    sz += chunk[i] - 'A' + 10;
                } else {
                    return;
                }
            }
            if (consumedBytes == 0 || chunk.size() < (sz + 1)) {
                isTruncated = true;
                return;
            }
            auto termcr = chunk[sz];
            if (!IsLfOrCr(termcr)) {
                return;
            }
            ++consumedBytes;
            if (chunk.size() >= (sz + 2)) {
                if (IsLfOrCr(chunk[sz + 1]) && chunk[sz] != chunk[sz + 1]) {
                    ++consumedBytes;
                }
            } else if (!crLfSingleMode) {
                isTruncated = true;
                return;
            }
            chunk.resize(sz);
            consumedBytes += sz;
            isValid = true;
        }
    }
    constexpr std::string GetChunk() const {
        return chunk;
    }
    constexpr bool IsValid() const {
        return isValid;
    }
    constexpr bool IsTruncated() const {
        return isTruncated;
    }
    constexpr size_t GetConsumedBytes() const {
        return consumedBytes;
    }
    constexpr std::string GetEncoded() const {
        auto sz = chunk.size();
        if (sz > 0) {
            std::string encoded{};
            while (sz > 0) {
                auto szv = sz & 0xF;
                sz = sz >> 4;
                char ch;
                if (szv < 10) {
                    ch = '0' + szv;
                } else {
                    ch = 'a' + szv - 10;
                }
                encoded.insert(0, &ch, 1);
            }
            encoded.append("\r\n");
            encoded.reserve(encoded.size() + chunk.size() + 2);
            encoded.append(chunk);
            encoded.append("\r\n");
            return encoded;
        } else {
            return "0\r\n\r\n";
        }
    }
};

class Http1ResponseLine {
private:
    std::string version{};
    std::string description{};
    int code{0};
    bool isValid{false};
public:
    static constexpr bool IsSpace(char ch) {
        return ch == ' ' || ch == '\t';
    }
    constexpr Http1ResponseLine() = default;
    constexpr Http1ResponseLine(const std::string &version, int code, const std::string &description) : version(version), description(description), code(code), isValid(true) {}
    constexpr Http1ResponseLine(std::string &&version, int code, std::string &&description) : version(std::move(version)), description(std::move(description)), code(code), isValid(true) {}
    constexpr Http1ResponseLine(const std::string &input) : version(input), description(), code(0), isValid(false) {
        {
            decltype(version.size()) i = 0;
            while (i < version.size() && IsSpace(version[i])) {
                ++i;
            }
            if (i > 0) {
                version.erase(0, i);
            }
        }
        for (decltype(version.size()) i = 0; i < version.size(); i++) {
            if (IsSpace(version[i])) {
                auto versionLength = i;
                for (i++; i < version.size(); i++) {
                    if (!IsSpace(version[i])) {
                        description = version.substr(i);
                        break;
                    }
                }
                version.resize(versionLength);
                break;
            }
        }
        for (decltype(description.size()) i = 0; i < description.size(); i++) {
            if (!IsSpace(description[i])) {
                if (i > 0) {
                    description.erase(0, i);
                }
                break;
            }
        }
        for (decltype(description.size()) i = 0; i < description.size(); i++) {
            if (description[i] >= '0' && description[i] <= '9') {
                code = code * 10;
                code += description[i] - '0';
            } else if (IsSpace(description[i])) {
                if (i == 0) {
                    return;
                }
                description.erase(0, i + 1);
                break;
            }
        }
        for (decltype(description.size()) i = 0; i < description.size(); i++) {
            if (!IsSpace(description[i])) {
                if (i > 0) {
                    description.erase(0, i);
                }
                break;
            }
        }
        isValid = !description.empty();
    }
    constexpr operator std::string () const {
        std::string ln{version};
        if (!ln.empty()) {
            ln.append(" ");
        }
        auto cd = code;
        if (cd > 0) {
            auto pos = ln.size();
            while (cd > 0) {
                auto dg = cd % 10;
                cd = cd / 10;
                char ch = '0' + dg;
                ln.insert(pos, &ch, 1);
            }
        } else {
            ln.append("0");
        }
        if (!description.empty()) {
            ln.reserve(ln.size() + description.size() + 1);
            ln.append(" ");
            ln.append(description);
        }
        return ln;
    }
    constexpr std::string GetVersion() const {
        return version;
    }
    constexpr std::string GetDescription() const {
        return description;
    }
    constexpr int GetCode() const {
        return code;
    }
    constexpr bool IsValid() const {
        return isValid;
    }
};

class Http1Response {
private:
    Http1ResponseLine responseLine{};
    std::vector<Http1HeaderLine> headerLines{};
public:
    constexpr Http1Response() = default;
    constexpr Http1Response(const Http1ResponseLine &responseLine, const std::vector<Http1HeaderLine> &headerLines) : responseLine(responseLine), headerLines(headerLines) {}
    constexpr Http1Response(Http1ResponseLine &&responseLine, std::vector<Http1HeaderLine> &&headerLines) : responseLine(std::move(responseLine)), headerLines(std::move(headerLines)) {}
    constexpr Http1ResponseLine GetResponseLine() const {
        return responseLine;
    }
    constexpr std::vector<Http1HeaderLine> GetHeader() const {
        return headerLines;
    }
    constexpr operator std::string () const {
        std::string str{responseLine.operator std::string()};
        if (str.empty()) {
            return str;
        }
        str.append("\r\n");
        for (const auto &hdr : headerLines) {
            str.append(hdr.operator std::string());
            str.append("\r\n");
        }
        str.append("\r\n");
        return str;
    }
};

class Http1ResponseParser {
private:
    Http1ResponseLine responseLine{};
    std::vector<Http1HeaderLine> headerLines{};
    size_t parsedInputCharacters{0};
    bool validHttpResponse{false};
    bool truncatedHttpResponse{false};
public:
    constexpr bool IsLfOrCr(char ch) {
        return ch == '\n' || ch == '\r';
    }
    constexpr Http1ResponseParser(const std::string &input) {
        if (input.empty()) {
            truncatedHttpResponse = true;
            return;
        }
        decltype(input.size()) i = 0;
        while (i < input.size() && !IsLfOrCr(input[i])) {
            ++i;
        }
        if (i == 0) {
            return;
        }
        if (i >= input.size()) {
            truncatedHttpResponse = true;
            return;
        }
        responseLine = Http1ResponseLine(input.substr(0, i));
        if (!responseLine.IsValid()) {
            return;
        }
        bool singleLfOrCr{false};
        {
            auto prevCh = input[i];
            ++i;
            if (i < input.size() && IsLfOrCr(input[i]) && input[i] != prevCh) {
                ++i;
            } else {
                singleLfOrCr = true;
            }
        }
        while (i < input.size() && !IsLfOrCr(input[i])) {
            decltype(i) start = i;
            ++i;
            while (i < input.size() && !IsLfOrCr(input[i])) {
                ++i;
            }
            if (i >= input.size()) {
                truncatedHttpResponse = true;
                return;
            }
            Http1HeaderLine hdr{input.substr(start, i - start)};
            if (hdr.GetHeader().empty()) {
                return;
            }
            auto prevCh = input[i];
            ++i;
            if (i < input.size() && IsLfOrCr(input[i]) && prevCh != input[i]) {
                ++i;
            }
            headerLines.emplace_back(std::move(hdr));
        }
        if (i >= input.size()) {
            truncatedHttpResponse = true;
            return;
        }
        auto prevCh = input[i];
        ++i;
        if (IsLfOrCr(input[i]) && input[i] != prevCh) {
            ++i;
        } else if (!singleLfOrCr) {
            truncatedHttpResponse = true;
            return;
        }
        validHttpResponse = true;
        parsedInputCharacters = i;
    }
    constexpr bool IsValid() const {
        return validHttpResponse;
    }
    constexpr bool IsTruncated() const {
        return truncatedHttpResponse;
    }
    constexpr size_t GetParsedInputCharacters() const {
        return parsedInputCharacters;
    }
    constexpr operator Http1Response () const {
        return {responseLine, headerLines};
    }
};

#endif //LIBHTTPTOOLING_HTTP1PROTOCOL_H
