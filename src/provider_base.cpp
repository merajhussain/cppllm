#include "internal/provider_base.hpp"

#include <algorithm>
#include <chrono>
#include <thread>

#include "llmwrapper/errors.hpp"

namespace llmwrapper::detail {

std::string joinEndpoint(const std::string& base, const std::string& suffix) {
    std::string b = base;
    while (!b.empty() && b.back() == '/') b.pop_back();
    if (b.size() >= suffix.size() && b.compare(b.size() - suffix.size(), suffix.size(), suffix) == 0) {
        return b;
    }
    return b + suffix;
}

std::string extractErrorMessage(const std::string& body) {
    const json parsed = json::parse(body, nullptr, /*allow_exceptions=*/false);
    if (!parsed.is_discarded() && parsed.is_object()) {
        // OpenAI / Anthropic / Gemini: {"error": {"message": "..."}}
        if (auto it = parsed.find("error"); it != parsed.end()) {
            if (it->is_object()) {
                if (auto m = it->find("message"); m != it->end() && m->is_string()) return m->get<std::string>();
            } else if (it->is_string()) {
                return it->get<std::string>();
            }
        }
        // Mistral and others: {"message": "..."} or {"detail": "..."}
        for (const char* key : {"message", "detail"}) {
            if (auto m = parsed.find(key); m != parsed.end()) {
                return m->is_string() ? m->get<std::string>() : m->dump();
            }
        }
    }
    if (body.empty()) return "empty response body";
    return body.size() > 500 ? body.substr(0, 500) + "..." : body;
}

ProviderServiceBase::ProviderServiceBase(Provider provider, ProviderConfig config,
                                         std::shared_ptr<HttpClient> http)
    : provider_(provider), config_(std::move(config)), http_(std::move(http)) {
    if (config_.apiKey.empty()) {
        throw ConfigError("No API key configured for provider '" + toString(provider_) + "'");
    }
    if (config_.model.empty()) config_.model = defaultModel(provider_);
    if (config_.baseUrl.empty()) config_.baseUrl = defaultBaseUrl(provider_);
    if (config_.maxRetries < 0) config_.maxRetries = 0;
    if (!http_) http_ = makeDefaultHttpClient();
}

std::string ProviderServiceBase::modelFor(const GenerationOptions& options) const {
    return options.model && !options.model->empty() ? *options.model : config_.model;
}

json ProviderServiceBase::postJson(const std::string& url, Headers headers, const json& body) const {
    HttpRequest request;
    request.method = "POST";
    request.url = url;
    request.headers = std::move(headers);
    request.headers.emplace_back("Content-Type", "application/json");
    request.headers.emplace_back("Accept", "application/json");
    request.body = body.dump(-1, ' ', /*ensure_ascii=*/false, json::error_handler_t::replace);
    request.timeout = config_.timeout;

    const std::string name = toString(provider_);
    auto backoff = std::chrono::milliseconds(500);

    for (int attempt = 0;; ++attempt) {
        const bool canRetry = attempt < config_.maxRetries;
        HttpResponse response;
        try {
            response = http_->send(request);
        } catch (const TransportError&) {
            if (!canRetry) throw;
            std::this_thread::sleep_for(backoff);
            backoff *= 2;
            continue;
        }

        if (response.status >= 200 && response.status < 300) {
            json parsed = json::parse(response.body, nullptr, /*allow_exceptions=*/false);
            if (parsed.is_discarded()) {
                throw ParseError(name + " returned a response that is not valid JSON");
            }
            return parsed;
        }

        ApiError error(name, response.status, extractErrorMessage(response.body), response.body);
        if (!error.isRetryable() || !canRetry) throw error;
        std::this_thread::sleep_for(backoff);
        backoff *= 2;
    }
}

}  // namespace llmwrapper::detail
