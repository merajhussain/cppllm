#include "llmwrapper/llm_service.hpp"

#include <algorithm>
#include <cctype>

#include "internal/provider_base.hpp"
#include "llmwrapper/errors.hpp"

namespace llmwrapper {

std::string LLMService::generateText(const std::string& prompt, const GenerationOptions& options) {
    return chat({Message::user(prompt)}, options).text;
}

Sentiment LLMService::analyzeSentiment(const std::string& text, const GenerationOptions& options) {
    const std::string prompt =
        "Analyze the sentiment of the following text and respond with only one word: "
        "POSITIVE, NEGATIVE, or NEUTRAL.\n\nText: " + text;
    return parseSentiment(generateText(prompt, options));
}

Sentiment parseSentiment(const std::string& reply) {
    std::string upper = reply;
    std::transform(upper.begin(), upper.end(), upper.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });

    // Pick whichever label appears first, so "POSITIVE (not negative)" is POSITIVE.
    Sentiment best = Sentiment::Unknown;
    std::size_t bestPos = std::string::npos;
    const std::pair<const char*, Sentiment> labels[] = {
        {"POSITIVE", Sentiment::Positive}, {"NEGATIVE", Sentiment::Negative}, {"NEUTRAL", Sentiment::Neutral}};
    for (const auto& [word, value] : labels) {
        const auto pos = upper.find(word);
        if (pos != std::string::npos && pos < bestPos) {
            bestPos = pos;
            best = value;
        }
    }
    return best;
}

std::unique_ptr<LLMService> createService(Provider provider, ProviderConfig config,
                                          std::shared_ptr<HttpClient> http) {
    switch (provider) {
        case Provider::OpenAI:
        case Provider::Mistral:
            return detail::makeOpenAICompatibleService(provider, std::move(config), std::move(http));
        case Provider::Anthropic:
            return detail::makeAnthropicService(std::move(config), std::move(http));
        case Provider::Gemini:
            return detail::makeGeminiService(std::move(config), std::move(http));
    }
    throw ConfigError("Unknown provider");
}

}  // namespace llmwrapper
