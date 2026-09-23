// OpenAI and Mistral both use the OpenAI "chat/completions" wire format.

#include "internal/provider_base.hpp"
#include "llmwrapper/errors.hpp"

namespace llmwrapper::detail {
namespace {

const char* roleName(Role role) {
    switch (role) {
        case Role::System: return "system";
        case Role::Assistant: return "assistant";
        case Role::User: break;
    }
    return "user";
}

// "content" is normally a string, but can be null or an array of parts.
std::string contentText(const json& content) {
    if (content.is_string()) return content.get<std::string>();
    std::string out;
    if (content.is_array()) {
        for (const auto& part : content) {
            if (part.is_object() && part.value("type", "") == "text" && part.contains("text") &&
                part["text"].is_string()) {
                out += part["text"].get<std::string>();
            }
        }
    }
    return out;
}

class OpenAICompatibleService final : public ProviderServiceBase {
public:
    using ProviderServiceBase::ProviderServiceBase;

    Response chat(const std::vector<Message>& messages, const GenerationOptions& options) override {
        json body;
        body["model"] = modelFor(options);
        body["messages"] = json::array();
        for (const auto& m : messages) {
            body["messages"].push_back({{"role", roleName(m.role)}, {"content", m.content}});
        }
        if (options.maxTokens) {
            // Newer OpenAI models (o-series, gpt-5) reject "max_tokens".
            body[provider() == Provider::OpenAI ? "max_completion_tokens" : "max_tokens"] = *options.maxTokens;
        }
        if (options.temperature) body["temperature"] = *options.temperature;
        if (options.topP) body["top_p"] = *options.topP;
        if (!options.stopSequences.empty()) body["stop"] = options.stopSequences;

        const Headers headers{{"Authorization", "Bearer " + config().apiKey}};
        const json result = postJson(joinEndpoint(config().baseUrl, "/chat/completions"), headers, body);

        const auto& choices = result.value("choices", json::array());
        if (!choices.is_array() || choices.empty() || !choices[0].is_object()) {
            throw ParseError(toString(provider()) + " response has no choices");
        }
        const auto& choice = choices[0];

        Response response;
        if (choice.contains("message") && choice["message"].is_object()) {
            response.text = contentText(choice["message"].value("content", json()));
        }
        if (choice.contains("finish_reason") && choice["finish_reason"].is_string()) {
            response.stopReason = choice["finish_reason"].get<std::string>();
        }
        response.model = result.value("model", std::string());
        if (result.contains("usage") && result["usage"].is_object()) {
            response.usage.inputTokens = result["usage"].value("prompt_tokens", 0);
            response.usage.outputTokens = result["usage"].value("completion_tokens", 0);
        }
        response.rawJson = result.dump();
        return response;
    }
};

}  // namespace

std::unique_ptr<LLMService> makeOpenAICompatibleService(Provider provider, ProviderConfig config,
                                                        std::shared_ptr<HttpClient> http) {
    return std::make_unique<OpenAICompatibleService>(provider, std::move(config), std::move(http));
}

}  // namespace llmwrapper::detail
