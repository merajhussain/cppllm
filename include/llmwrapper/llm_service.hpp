#pragma once

#include <memory>
#include <string>
#include <vector>

#include "llmwrapper/config.hpp"
#include "llmwrapper/export.hpp"
#include "llmwrapper/http_client.hpp"
#include "llmwrapper/types.hpp"

namespace llmwrapper {

/// One LLM provider (C++ counterpart of the Java LLMService interface).
/// Instances are immutable after construction and safe to share between threads.
class LLMWRAPPER_API LLMService {
public:
    virtual ~LLMService() = default;

    virtual Provider provider() const = 0;

    /// Full chat call: multi-turn history, system prompt, options, usage.
    virtual Response chat(const std::vector<Message>& messages,
                          const GenerationOptions& options = {}) = 0;

    /// Single prompt in, text out (the Java generateText).
    std::string generateText(const std::string& prompt, const GenerationOptions& options = {});

    /// Classifies text as POSITIVE / NEGATIVE / NEUTRAL. Works the same on every provider.
    Sentiment analyzeSentiment(const std::string& text, const GenerationOptions& options = {});
};

/// Creates a provider service (the Java LLMServiceFactory). If `http` is null,
/// makeDefaultHttpClient() is used. Throws ConfigError if apiKey is empty.
LLMWRAPPER_API std::unique_ptr<LLMService> createService(Provider provider, ProviderConfig config,
                                                         std::shared_ptr<HttpClient> http = nullptr);

/// Parses the model's reply to the sentiment prompt. Exposed for testing.
LLMWRAPPER_API Sentiment parseSentiment(const std::string& reply);

}  // namespace llmwrapper
