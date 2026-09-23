#include "internal/provider_base.hpp"
#include "llmwrapper/errors.hpp"
#include<iostream>
#include <string>

namespace llmwrapper::detail {
namespace {

constexpr int kDefaultMaxTokens = 1024;  // Same as the Java MAX_TOKENS; the API requires a value.
constexpr const char* kApiVersion = "2023-06-01";

class AnthropicService final : public ProviderServiceBase {
public:
    AnthropicService(ProviderConfig config, std::shared_ptr<HttpClient> http)
        : ProviderServiceBase(Provider::Anthropic, std::move(config), std::move(http)) {}

    Response chat(const std::vector<Message>& messages, const GenerationOptions& options) override {
        json body;
        body["model"] = modelFor(options);
        body["max_tokens"] = options.maxTokens.value_or(kDefaultMaxTokens);
        body["messages"] = json::array();

        // Anthropic takes the system prompt as a top-level field, not a message.
        std::string system;
        for (const auto& m : messages) {
            if (m.role == Role::System) {
                if (!system.empty()) system += "\n\n";
                system += m.content;
            } else {
                body["messages"].push_back(
                    {{"role", m.role == Role::Assistant ? "assistant" : "user"}, {"content", m.content}});
            }
        }
        if (!system.empty()) body["system"] = system;
        if (options.temperature) body["temperature"] = *options.temperature;
        if (options.topP) body["top_p"] = *options.topP;
        if (!options.stopSequences.empty()) body["stop_sequences"] = options.stopSequences;

        const Headers headers{{"x-api-key", config().apiKey}, {"anthropic-version", kApiVersion}};
        const json result = postJson(joinEndpoint(config().baseUrl, "/messages"), headers, body);

        if (!result.contains("content") || !result["content"].is_array()) {
            throw ParseError("anthropic response has no content");
        }

        Response response;
        for (const auto& block : result["content"]) {
            if (block.is_object() && block.value("type", "") == "text" && block.contains("text") &&
                block["text"].is_string()) {
                response.text += block["text"].get<std::string>();
            }
        }
        response.model = result.value("model", std::string());
        if (result.contains("stop_reason") && result["stop_reason"].is_string()) {
            response.stopReason = result["stop_reason"].get<std::string>();
        }
        if (result.contains("usage") && result["usage"].is_object()) {
            response.usage.inputTokens = result["usage"].value("input_tokens", 0);
            response.usage.outputTokens = result["usage"].value("output_tokens", 0);
        }
        response.rawJson = result.dump();
        std::cout <<response.text << std::endl;
        
        return response;
    }
};

}  // namespace

std::unique_ptr<LLMService> makeAnthropicService(ProviderConfig config, std::shared_ptr<HttpClient> http) {
    return std::make_unique<AnthropicService>(std::move(config), std::move(http));
}

}  // namespace llmwrapper::detail
