#include "llmwrapper/types.hpp"

#include <algorithm>
#include <cctype>

#include "llmwrapper/errors.hpp"

namespace llmwrapper {

std::string toString(Provider provider) {
    switch (provider) {
        case Provider::OpenAI: return "openai";
        case Provider::Anthropic: return "anthropic";
        case Provider::Gemini: return "gemini";
        case Provider::Mistral: return "mistral";
    }
    return "unknown";
}

Provider parseProvider(const std::string& name) {
    std::string n = name;
    std::transform(n.begin(), n.end(), n.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (n == "openai") return Provider::OpenAI;
    if (n == "anthropic" || n == "claude") return Provider::Anthropic;
    if (n == "gemini" || n == "google") return Provider::Gemini;
    if (n == "mistral") return Provider::Mistral;
    throw ConfigError("Unknown provider: '" + name + "' (expected openai, anthropic, gemini or mistral)");
}

std::string toString(Sentiment sentiment) {
    switch (sentiment) {
        case Sentiment::Positive: return "POSITIVE";
        case Sentiment::Negative: return "NEGATIVE";
        case Sentiment::Neutral: return "NEUTRAL";
        case Sentiment::Unknown: break;
    }
    return "UNKNOWN";
}

}  // namespace llmwrapper
