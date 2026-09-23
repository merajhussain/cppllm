// WinHTTP transport: the default on Windows. WinHTTP ships with the OS, so no
// OpenSSL / libcurl is needed, and it honours the system proxy settings and
// certificate store. Linux/macOS use curl_client.cpp instead.

#include "llmwrapper/errors.hpp"
#include "llmwrapper/http_client.hpp"

#if defined(_WIN32) && !defined(LLMWRAPPER_USE_CURL)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winhttp.h>

#if defined(_MSC_VER)
// Pull in winhttp.lib automatically, so apps linking llmwrapper.lib don't have to.
#pragma comment(lib, "winhttp.lib")
#endif

#include <limits>
#include <string>

namespace llmwrapper {
namespace {

std::wstring widen(const std::string& s) {
    if (s.empty()) return {};
    const int len = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring out(static_cast<std::size_t>(len), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), len);
    return out;
}

std::string narrow(const std::wstring& s) {
    if (s.empty()) return {};
    const int len =
        WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<std::size_t>(len), '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), len, nullptr, nullptr);
    return out;
}

std::string describeError(DWORD code) {
    wchar_t* buffer = nullptr;
    DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_SYSTEM;
    HMODULE winhttp = GetModuleHandleW(L"winhttp.dll");
    if (winhttp) flags |= FORMAT_MESSAGE_FROM_HMODULE;  // WinHTTP error texts live in the DLL
    FormatMessageW(flags, winhttp, code, 0, reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);
    std::string text;
    if (buffer) {
        text = narrow(buffer);
        LocalFree(buffer);
        while (!text.empty() && (text.back() == '\n' || text.back() == '\r' || text.back() == ' ')) text.pop_back();
    }
    if (text.empty()) text = "WinHTTP error";
    return text + " (code " + std::to_string(code) + ")";
}

[[noreturn]] void fail(const std::string& what) {
    const DWORD code = GetLastError();
    throw TransportError(what + ": " + describeError(code), code);
}

struct InternetHandle {
    HINTERNET h = nullptr;
    explicit InternetHandle(HINTERNET handle = nullptr) : h(handle) {}
    ~InternetHandle() {
        if (h) WinHttpCloseHandle(h);
    }
    InternetHandle(const InternetHandle&) = delete;
    InternetHandle& operator=(const InternetHandle&) = delete;
};

class WinHttpClient final : public HttpClient {
public:
    WinHttpClient() {
#if defined(WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY)
        // Windows 8.1+: uses system / IE proxy settings including PAC scripts.
        session_.h = WinHttpOpen(L"llmwrapper-cpp/3.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                 WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
#endif
        if (!session_.h) {
            session_.h = WinHttpOpen(L"llmwrapper-cpp/3.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        }
        if (!session_.h) fail("WinHttpOpen failed");

        DWORD protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
#if defined(WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3)
        DWORD withTls13 = protocols | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3;
        if (!WinHttpSetOption(session_.h, WINHTTP_OPTION_SECURE_PROTOCOLS, &withTls13, sizeof(withTls13)))
#endif
            WinHttpSetOption(session_.h, WINHTTP_OPTION_SECURE_PROTOCOLS, &protocols, sizeof(protocols));

#if defined(WINHTTP_OPTION_DECOMPRESSION) && defined(WINHTTP_DECOMPRESSION_FLAG_ALL)
        DWORD decompression = WINHTTP_DECOMPRESSION_FLAG_ALL;  // gzip/deflate, best effort
        WinHttpSetOption(session_.h, WINHTTP_OPTION_DECOMPRESSION, &decompression, sizeof(decompression));
#endif
    }

    HttpResponse send(const HttpRequest& req) override {
        const std::wstring url = widen(req.url);

        URL_COMPONENTS parts{};
        parts.dwStructSize = sizeof(parts);
        parts.dwSchemeLength = static_cast<DWORD>(-1);
        parts.dwHostNameLength = static_cast<DWORD>(-1);
        parts.dwUrlPathLength = static_cast<DWORD>(-1);
        parts.dwExtraInfoLength = static_cast<DWORD>(-1);
        if (!WinHttpCrackUrl(url.c_str(), 0, 0, &parts)) fail("Invalid URL '" + req.url + "'");

        const std::wstring host(parts.lpszHostName, parts.dwHostNameLength);
        std::wstring path(parts.lpszUrlPath, parts.dwUrlPathLength);
        if (parts.lpszExtraInfo) path.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);
        if (path.empty()) path = L"/";
        const bool secure = parts.nScheme == INTERNET_SCHEME_HTTPS;

        InternetHandle connection(WinHttpConnect(session_.h, host.c_str(), parts.nPort, 0));
        if (!connection.h) fail("WinHttpConnect failed");

        const std::wstring method = widen(req.method.empty() ? std::string("POST") : req.method);
        InternetHandle request(WinHttpOpenRequest(connection.h, method.c_str(), path.c_str(), nullptr,
                                                  WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                                  secure ? WINHTTP_FLAG_SECURE : 0));
        if (!request.h) fail("WinHttpOpenRequest failed");

        const auto ms = req.timeout.count();
        const int timeout =
            ms <= 0 ? 0 : static_cast<int>(ms > std::numeric_limits<int>::max() ? std::numeric_limits<int>::max() : ms);
        WinHttpSetTimeouts(request.h, timeout, timeout, timeout, timeout);

        std::wstring headers;
        for (const auto& [name, value] : req.headers) {
            headers += widen(name) + L": " + widen(value) + L"\r\n";
        }

        if (req.body.size() > std::numeric_limits<DWORD>::max()) {
            throw TransportError("Request body too large");
        }
        const DWORD bodySize = static_cast<DWORD>(req.body.size());
        LPVOID bodyData = bodySize ? const_cast<char*>(req.body.data()) : WINHTTP_NO_REQUEST_DATA;

        if (!WinHttpSendRequest(request.h, headers.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : headers.c_str(),
                                headers.empty() ? 0 : static_cast<DWORD>(-1L), bodyData, bodySize, bodySize, 0)) {
            fail("Request to " + req.url + " failed");
        }
        if (!WinHttpReceiveResponse(request.h, nullptr)) fail("No response from " + req.url);

        HttpResponse response;
        DWORD status = 0;
        DWORD statusSize = sizeof(status);
        if (!WinHttpQueryHeaders(request.h, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                 WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX)) {
            fail("Could not read HTTP status");
        }
        response.status = static_cast<int>(status);

        for (;;) {
            DWORD available = 0;
            if (!WinHttpQueryDataAvailable(request.h, &available)) fail("Reading response failed");
            if (available == 0) break;
            const std::size_t offset = response.body.size();
            response.body.resize(offset + available);
            DWORD read = 0;
            if (!WinHttpReadData(request.h, response.body.data() + offset, available, &read)) {
                fail("Reading response failed");
            }
            response.body.resize(offset + read);
        }
        return response;
    }

private:
    InternetHandle session_;
};

}  // namespace

std::shared_ptr<HttpClient> makeDefaultHttpClient() { return std::make_shared<WinHttpClient>(); }

}  // namespace llmwrapper

#elif !defined(LLMWRAPPER_USE_CURL)  // no built-in transport

namespace llmwrapper {

std::shared_ptr<HttpClient> makeDefaultHttpClient() {
    throw ConfigError(
        "llmwrapper was built without an HTTP transport. Rebuild with "
        "-DLLMWRAPPER_USE_CURL=ON (needs libcurl), or implement llmwrapper::HttpClient "
        "and pass it to LLMClient / createService.");
}

}  // namespace llmwrapper

#endif
