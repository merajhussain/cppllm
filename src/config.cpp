#include "llmwrapper/config.hpp"

#include <cstdlib>
#include <fstream>

#include "llmwrapper/errors.hpp"

namespace llmwrapper {
namespace {

std::string trim(const std::string& s) {
    const auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const auto last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last - first + 1);
}

std::string getEnv(const std::string& name) {
#if defined(_MSC_VER)
    char* value = nullptr;
    std::size_t len = 0;
    if (_dupenv_s(&value, &len, name.c_str()) != 0 || value == nullptr) return {};
    std::string result(value);
    std::free(value);
    return result;
#else
    const char* value = std::getenv(name.c_str());
    return value ? std::string(value) : std::string();
#endif
}

std::string lookup(const std::string& name, const std::map<std::string, std::string>& overrides) {
    if (auto it = overrides.find(name); it != overrides.end() && !it->second.empty()) return it->second;
    return getEnv(name);
}

}  // namespace

std::string defaultModel(Provider provider) {
    switch (provider) {
        case Provider::OpenAI: return "gpt-4o-mini";
        case Provider::Anthropic: return "claude-sonnet-4-5";
        case Provider::Gemini: return "gemini-2.5-flash";
        case Provider::Mistral: return "mistral-small-latest";
    }
    return {};
}

std::string defaultBaseUrl(Provider provider) {
    switch (provider) {
        case Provider::OpenAI: return "https://api.openai.com/v1";
        case Provider::Anthropic: return "https://api.anthropic.com/v1";
        case Provider::Gemini: return "https://generativelanguage.googleapis.com/v1beta";
        case Provider::Mistral: return "https://api.mistral.ai/v1";
    }
    return {};
}

std::map<std::string, std::string> loadDotEnv(const std::string& path) {
    std::ifstream file(path);
    if (!file) throw ConfigError("Cannot open .env file: " + path);

    std::map<std::string, std::string> values;
    std::string line;
    bool first = true;
    while (std::getline(file, line)) {
        if (first) {  // Strip a UTF-8 BOM (Notepad likes to add one).
            if (line.rfind("\xEF\xBB\xBF", 0) == 0) line.erase(0, 3);
            first = false;
        }
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        if (line.rfind("export ", 0) == 0) line = trim(line.substr(7));

        const auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = trim(line.substr(0, eq));
        std::string value = trim(line.substr(eq + 1));

        if (value.size() >= 2 && (value.front() == '"' || value.front() == '\'') && value.back() == value.front()) {
            value = value.substr(1, value.size() - 2);
        } else if (const auto hash = value.find(" #"); hash != std::string::npos) {
            value = trim(value.substr(0, hash));  // inline comment
        }
        if (!key.empty()) values[key] = value;
    }
    return values;
}

std::optional<ProviderConfig> configFromEnvironment(Provider provider,
                                                    const std::map<std::string, std::string>& overrides) {
    std::string prefix;
    std::string urlVar;
    switch (provider) {
        case Provider::OpenAI: prefix = "OPENAI"; urlVar = "OPENAI_BASE_URL"; break;
        case Provider::Anthropic: prefix = "ANTHROPIC"; urlVar = "ANTHROPIC_API_URL"; break;
        case Provider::Gemini: prefix = "GEMINI"; urlVar = "GEMINI_BASE_URL"; break;
        case Provider::Mistral: prefix = "MISTRAL"; urlVar = "MISTRAL_BASE_URL"; break;
    }

    ProviderConfig config;
    config.apiKey = lookup(prefix + "_API_KEY", overrides);
    if (config.apiKey.empty()) return std::nullopt;
    config.model = lookup(prefix + "_MODEL", overrides);
    config.baseUrl = lookup(urlVar, overrides);
    return config;
}

}  // namespace llmwrapper
