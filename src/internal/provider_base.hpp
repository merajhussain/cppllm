#pragma once

// Internal helpers shared by the provider implementations. Not installed.

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "llmwrapper/config.hpp"
#include "llmwrapper/http_client.hpp"
#include "llmwrapper/llm_service.hpp"
#include "nlohmann/json.hpp"

namespace llmwrapper::detail {

using json = nlohmann::json;
using Headers = std::vector<std::pair<std::string, std::string>>;

/// Appends `suffix` to `base` unless `base` already ends with it.
std::string joinEndpoint(const std::string& base, const std::string& suffix);

/// Pulls a human-readable error message out of a provider error body.
std::string extractErrorMessage(const std::string& body);

/// Common plumbing: config, transport, retries, error mapping.
class ProviderServiceBase : public LLMService {
public:
    ProviderServiceBase(Provider provider, ProviderConfig config, std::shared_ptr<HttpClient> http);

    Provider provider() const override { return provider_; }

protected:
    const ProviderConfig& config() const { return config_; }
    std::string modelFor(const GenerationOptions& options) const;

    /// POSTs JSON, retries on 429/5xx/transport errors, throws ApiError on
    /// a final non-2xx, returns the parsed JSON body.
    json postJson(const std::string& url, Headers headers, const json& body) const;

private:
    Provider provider_;
    ProviderConfig config_;
    std::shared_ptr<HttpClient> http_;
};

std::unique_ptr<LLMService> makeOpenAICompatibleService(Provider provider, ProviderConfig config,
                                                        std::shared_ptr<HttpClient> http);
std::unique_ptr<LLMService> makeAnthropicService(ProviderConfig config, std::shared_ptr<HttpClient> http);
std::unique_ptr<LLMService> makeGeminiService(ProviderConfig config, std::shared_ptr<HttpClient> http);

}  // namespace llmwrapper::detail
