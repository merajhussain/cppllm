#pragma once

#include <chrono>
#include <map>
#include <optional>
#include <string>

#include "llmwrapper/export.hpp"
#include "llmwrapper/types.hpp"

namespace llmwrapper {

/// Settings for one provider (replaces the Java *Config classes).
struct ProviderConfig {
    std::string apiKey;
    std::string model;     ///< Empty = library default for the provider.
    std::string baseUrl;   ///< Empty = official endpoint. Set for proxies / Azure / self-hosted.
    std::chrono::milliseconds timeout{60000};
    int maxRetries = 2;    ///< Retries on 429 / 5xx / network errors, with exponential backoff.
};

/// Default model used when ProviderConfig::model is empty.
LLMWRAPPER_API std::string defaultModel(Provider provider);

/// Default endpoint used when ProviderConfig::baseUrl is empty.
LLMWRAPPER_API std::string defaultBaseUrl(Provider provider);

/// Parses a .env file (KEY=VALUE, '#' comments, optional quotes, optional
/// "export " prefix). Throws ConfigError if the file can't be opened.
LLMWRAPPER_API std::map<std::string, std::string> loadDotEnv(const std::string& path);

/// Builds a ProviderConfig from variables, using the same names as the Java
/// project's .env file:
///   OPENAI_API_KEY,    OPENAI_MODEL,    OPENAI_BASE_URL
///   ANTHROPIC_API_KEY, ANTHROPIC_MODEL, ANTHROPIC_API_URL
///   GEMINI_API_KEY,    GEMINI_MODEL,    GEMINI_BASE_URL
///   MISTRAL_API_KEY,   MISTRAL_MODEL,   MISTRAL_BASE_URL
/// Values in `overrides` (e.g. from loadDotEnv) win over process environment
/// variables. Returns std::nullopt when no API key is found.
LLMWRAPPER_API std::optional<ProviderConfig> configFromEnvironment(
    Provider provider, const std::map<std::string, std::string>& overrides = {});

}  // namespace llmwrapper
