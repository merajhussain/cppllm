// libcurl HTTP transport: the default on Linux and macOS. It can also be used
// on Windows by configuring CMake with -DLLMWRAPPER_USE_CURL=ON.
//
// Proxy settings come from the usual environment variables (https_proxy,
// no_proxy, ...), and TLS uses the system certificate store.

#if defined(LLMWRAPPER_USE_CURL)

#include <curl/curl.h>

#include <memory>
#include <mutex>
#include <string>

#include "llmwrapper/errors.hpp"
#include "llmwrapper/http_client.hpp"
#include "llmwrapper/version.hpp"

namespace llmwrapper {
namespace {

void globalInitOnce() {
    static std::once_flag once;
    std::call_once(once, [] {
        const CURLcode rc = curl_global_init(CURL_GLOBAL_DEFAULT);
        if (rc != CURLE_OK) {
            throw TransportError(std::string("curl_global_init failed: ") + curl_easy_strerror(rc),
                                 static_cast<unsigned long>(rc));
        }
    });
}

struct EasyDeleter {
    void operator()(CURL* handle) const noexcept { curl_easy_cleanup(handle); }
};
struct SlistDeleter {
    void operator()(curl_slist* list) const noexcept { curl_slist_free_all(list); }
};
using EasyHandle = std::unique_ptr<CURL, EasyDeleter>;
using HeaderList = std::unique_ptr<curl_slist, SlistDeleter>;

std::size_t writeBody(char* data, std::size_t size, std::size_t count, void* userdata) {
    auto* body = static_cast<std::string*>(userdata);
    body->append(data, size * count);
    return size * count;
}

// One easy handle per thread, reset between requests. Easy handles must not
// be shared between threads, but reusing one per thread keeps connections
// (and TLS sessions) alive between calls to the same provider.
CURL* threadHandle() {
    thread_local EasyHandle handle;
    if (!handle) {
        handle.reset(curl_easy_init());
        if (!handle) throw TransportError("curl_easy_init failed");
    } else {
        curl_easy_reset(handle.get());
    }
    return handle.get();
}

class CurlHttpClient final : public HttpClient {
public:
    CurlHttpClient() { globalInitOnce(); }

    HttpResponse send(const HttpRequest& req) override {
        CURL* curl = threadHandle();
        HttpResponse response;
        char errorBuffer[CURL_ERROR_SIZE] = {};

        HeaderList headers;
        auto appendHeader = [&headers](const std::string& line) {
            curl_slist* next = curl_slist_append(headers.get(), line.c_str());
            if (!next) throw TransportError("curl_slist_append failed");
            headers.release();
            headers.reset(next);
        };
        for (const auto& [name, value] : req.headers) appendHeader(name + ": " + value);
        appendHeader("Expect:");  // no "100-continue" round trip for larger bodies

        const std::string method = req.method.empty() ? std::string("POST") : req.method;

        curl_easy_setopt(curl, CURLOPT_URL, req.url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers.get());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &writeBody);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);
        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "llmwrapper-cpp/" LLMWRAPPER_VERSION_STRING);
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);          // required for multithreaded use
        curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");   // accept any compression curl supports
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

        const long timeoutMs = static_cast<long>(req.timeout.count());
        if (timeoutMs > 0) {
            curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, timeoutMs);
            curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, timeoutMs);
        }

        if (method == "GET") {
            curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
        } else {
            if (method != "POST") curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method.c_str());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, req.body.data());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(req.body.size()));
        }

        const CURLcode rc = curl_easy_perform(curl);
        // Don't leave pointers to this stack frame in the reused handle.
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, nullptr);
        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, nullptr);

        if (rc != CURLE_OK) {
            std::string detail = errorBuffer[0] ? std::string(errorBuffer) : std::string(curl_easy_strerror(rc));
            throw TransportError("Request to " + req.url + " failed: " + detail, static_cast<unsigned long>(rc));
        }

        long status = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
        response.status = static_cast<int>(status);
        return response;
    }
};

}  // namespace

std::shared_ptr<HttpClient> makeDefaultHttpClient() { return std::make_shared<CurlHttpClient>(); }

}  // namespace llmwrapper

#endif  // LLMWRAPPER_USE_CURL
