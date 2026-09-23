#pragma once

#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "llmwrapper/config.hpp"
#include "llmwrapper/export.hpp"
#include "llmwrapper/http_client.hpp"
#include "llmwrapper/llm_service.hpp"
#include "llmwrapper/types.hpp"

namespace llmwrapper {

/// Holds one service per provider and routes calls by provider
/// (the Java LLMServiceFacade + LLMController). Thread-safe.
class LLMWRAPPER_API LLMClient {
public:
    explicit LLMClient(std::shared_ptr<HttpClient> http = nullptr);

    /// Registers every provider that has an API key in the environment
    /// and/or in the given .env file (pass "" to skip the file). A missing
    /// .env file is not an error.
    static LLMClient fromEnvironment(const std::string& dotEnvPath = ".env",
                                     std::shared_ptr<HttpClient> http = nullptr);

    LLMClient(LLMClient&& other) noexcept;
    LLMClient& operator=(LLMClient&& other) noexcept;
    LLMClient(const LLMClient&) = delete;
    LLMClient& operator=(const LLMClient&) = delete;

    /// Adds or replaces a provider.
    void addProvider(Provider provider, ProviderConfig config);
    void addService(std::shared_ptr<LLMService> service);

    bool hasProvider(Provider provider) const;
    std::vector<Provider> providers() const;

    /// Throws ConfigError if the provider was not registered.
    std::shared_ptr<LLMService> service(Provider provider) const;

    Response chat(Provider provider, const std::vector<Message>& messages,
                  const GenerationOptions& options = {}) const;
    std::string generateText(Provider provider, const std::string& prompt,
                             const GenerationOptions& options = {}) const;
    Sentiment analyzeSentiment(Provider provider, const std::string& text,
                               const GenerationOptions& options = {}) const;

    /// Same as above, provider given by name ("openai", "anthropic", ...),
    /// matching the "model" field of the Java REST request.
    std::string generateText(const std::string& providerName, const std::string& prompt,
                             const GenerationOptions& options = {}) const;

    /// Runs chat() on a background thread.
    std::future<Response> chatAsync(Provider provider, std::vector<Message> messages,
                                    GenerationOptions options = {}) const;

private:
    std::shared_ptr<HttpClient> http_;
    mutable std::unique_ptr<std::mutex> mutex_;
    std::map<Provider, std::shared_ptr<LLMService>> services_;
};

}  // namespace llmwrapper
