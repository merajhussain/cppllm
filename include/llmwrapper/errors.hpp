#pragma once

#include <stdexcept>
#include <string>

#include "llmwrapper/export.hpp"

namespace llmwrapper {

/// Base class for every exception thrown by the library.
class LLMWRAPPER_API LLMError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

/// Missing API key, unknown provider, bad .env file, ...
class LLMWRAPPER_API ConfigError : public LLMError {
public:
    using LLMError::LLMError;
};

/// Network-level failure: DNS, TLS, timeout, connection reset.
class LLMWRAPPER_API TransportError : public LLMError {
public:
    TransportError(const std::string& message, unsigned long systemCode = 0)
        : LLMError(message), systemCode_(systemCode) {}
    /// OS error code (GetLastError() on Windows), 0 if not applicable.
    unsigned long systemCode() const noexcept { return systemCode_; }

private:
    unsigned long systemCode_;
};

/// The provider answered with a non-2xx HTTP status.
class LLMWRAPPER_API ApiError : public LLMError {
public:
    ApiError(const std::string& provider, int status, const std::string& message, std::string body)
        : LLMError(provider + " API error (HTTP " + std::to_string(status) + "): " + message),
          status_(status), body_(std::move(body)) {}
    int status() const noexcept { return status_; }
    const std::string& body() const noexcept { return body_; }
    /// 429 and 5xx are usually worth retrying.
    bool isRetryable() const noexcept { return status_ == 429 || status_ >= 500; }

private:
    int status_;
    std::string body_;
};

/// The provider answered 2xx but the body was not what we expected.
class LLMWRAPPER_API ParseError : public LLMError {
public:
    using LLMError::LLMError;
};

}  // namespace llmwrapper
