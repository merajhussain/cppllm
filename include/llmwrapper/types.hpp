#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "llmwrapper/export.hpp"

namespace llmwrapper {

/// Supported LLM providers.
enum class Provider { OpenAI, Anthropic, Gemini, Mistral };

/// "openai", "anthropic", "gemini", "mistral"
LLMWRAPPER_API std::string toString(Provider provider);

/// Case-insensitive. Accepts "openai", "anthropic"/"claude", "gemini"/"google",
/// "mistral". Throws llmwrapper::ConfigError for anything else.
LLMWRAPPER_API Provider parseProvider(const std::string& name);

enum class Role { System, User, Assistant };

struct Message {
    Role role = Role::User;
    std::string content;

    static Message system(std::string text) { return {Role::System, std::move(text)}; }
    static Message user(std::string text) { return {Role::User, std::move(text)}; }
    static Message assistant(std::string text) { return {Role::Assistant, std::move(text)}; }
};

/// Per-request options. Anything left empty uses the provider's default.
struct GenerationOptions {
    std::optional<std::string> model;       ///< Overrides ProviderConfig::model for one call.
    std::optional<int> maxTokens;           ///< Anthropic needs one; defaults to 1024 there.
    std::optional<double> temperature;
    std::optional<double> topP;
    std::vector<std::string> stopSequences;
};

struct Usage {
    int inputTokens = 0;
    int outputTokens = 0;
    int totalTokens() const { return inputTokens + outputTokens; }
};

struct Response {
    std::string text;         ///< Generated text (all text parts joined).
    std::string model;        ///< Model that answered, as reported by the provider.
    std::string stopReason;   ///< e.g. "stop", "end_turn", "max_tokens", "STOP".
    Usage usage;
    std::string rawJson;      ///< Full provider response body, for anything not mapped above.
};

enum class Sentiment { Positive, Negative, Neutral, Unknown };

LLMWRAPPER_API std::string toString(Sentiment sentiment);

}  // namespace llmwrapper
