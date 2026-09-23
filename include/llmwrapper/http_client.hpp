#pragma once

#include <chrono>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "llmwrapper/export.hpp"

namespace llmwrapper {

struct HttpRequest {
    std::string method = "POST";
    std::string url;
    std::vector<std::pair<std::string, std::string>> headers;
    std::string body;
    std::chrono::milliseconds timeout{60000};
};

struct HttpResponse {
    int status = 0;
    std::string body;
};

/// Transport abstraction. The library ships a WinHTTP implementation on
/// Windows; implement this yourself to plug in libcurl, a proxy, a mock for
/// tests, etc. Implementations must be safe to call from several threads.
class LLMWRAPPER_API HttpClient {
public:
    virtual ~HttpClient() = default;
    /// Returns any HTTP status (including 4xx/5xx). Throws TransportError
    /// only when no response could be obtained at all.
    virtual HttpResponse send(const HttpRequest& request) = 0;
};

/// The platform default: WinHTTP on Windows. Throws ConfigError on platforms
/// without a built-in transport (supply your own HttpClient there).
LLMWRAPPER_API std::shared_ptr<HttpClient> makeDefaultHttpClient();

}  // namespace llmwrapper
