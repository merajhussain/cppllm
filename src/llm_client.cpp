#include "llmwrapper/llm_client.hpp"

#include <fstream>

#include "llmwrapper/errors.hpp"

namespace llmwrapper {

LLMClient::LLMClient(std::shared_ptr<HttpClient> http)
    : http_(std::move(http)), mutex_(std::make_unique<std::mutex>()) {}

LLMClient::LLMClient(LLMClient&& other) noexcept = default;
LLMClient& LLMClient::operator=(LLMClient&& other) noexcept = default;

LLMClient LLMClient::fromEnvironment(const std::string& dotEnvPath, std::shared_ptr<HttpClient> http) {
    std::map<std::string, std::string> fileValues;
    if (!dotEnvPath.empty() && std::ifstream(dotEnvPath).good()) {
        fileValues = loadDotEnv(dotEnvPath);
    }

    LLMClient client(std::move(http));
    for (Provider p : {Provider::OpenAI, Provider::Anthropic, Provider::Gemini, Provider::Mistral}) {
        if (auto config = configFromEnvironment(p, fileValues)) {
            client.addProvider(p, std::move(*config));
        }
    }
    return client;
}

void LLMClient::addProvider(Provider provider, ProviderConfig config) {
    if (!http_) http_ = makeDefaultHttpClient();  // one shared transport for all providers
    addService(createService(provider, std::move(config), http_));
}

void LLMClient::addService(std::shared_ptr<LLMService> service) {
    if (!service) throw ConfigError("addService: service is null");
    std::lock_guard<std::mutex> lock(*mutex_);
    services_[service->provider()] = std::move(service);
}

bool LLMClient::hasProvider(Provider provider) const {
    std::lock_guard<std::mutex> lock(*mutex_);
    return services_.count(provider) != 0;
}

std::vector<Provider> LLMClient::providers() const {
    std::lock_guard<std::mutex> lock(*mutex_);
    std::vector<Provider> out;
    for (const auto& entry : services_) out.push_back(entry.first);
    return out;
}

std::shared_ptr<LLMService> LLMClient::service(Provider provider) const {
    std::lock_guard<std::mutex> lock(*mutex_);
    auto it = services_.find(provider);
    if (it == services_.end()) {
        throw ConfigError("Provider '" + toString(provider) + "' is not configured (missing API key?)");
    }
    return it->second;
}

Response LLMClient::chat(Provider provider, const std::vector<Message>& messages,
                         const GenerationOptions& options) const {
    return service(provider)->chat(messages, options);
}

std::string LLMClient::generateText(Provider provider, const std::string& prompt,
                                    const GenerationOptions& options) const {
    return service(provider)->generateText(prompt, options);
}

Sentiment LLMClient::analyzeSentiment(Provider provider, const std::string& text,
                                      const GenerationOptions& options) const {
    return service(provider)->analyzeSentiment(text, options);
}

std::string LLMClient::generateText(const std::string& providerName, const std::string& prompt,
                                    const GenerationOptions& options) const {
    return generateText(parseProvider(providerName), prompt, options);
}

std::future<Response> LLMClient::chatAsync(Provider provider, std::vector<Message> messages,
                                           GenerationOptions options) const {
    // Resolve the service now so a missing provider fails fast on the caller's thread.
    auto svc = service(provider);
    return std::async(std::launch::async,
                      [svc = std::move(svc), messages = std::move(messages), options = std::move(options)] {
                          return svc->chat(messages, options);
                      });
}

}  // namespace llmwrapper
