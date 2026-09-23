#include "internal/provider_base.hpp"
#include "llmwrapper/errors.hpp"

namespace llmwrapper::detail {
namespace {

// Accepts both "gemini-2.5-flash" and "models/gemini-2.5-flash".
std::string normalizeModel(std::string model) {
    const std::string prefix = "models/";
    if (model.rfind(prefix, 0) == 0) model.erase(0, prefix.size());
    return model;
}

class GeminiService final : public ProviderServiceBase {
public:
    GeminiService(ProviderConfig config, std::shared_ptr<HttpClient> http)
        : ProviderServiceBase(Provider::Gemini, std::move(config), std::move(http)) {}

    Response chat(const std::vector<Message>& messages, const GenerationOptions& options) override {
        json body;
        body["contents"] = json::array();

        std::string system;
        for (const auto& m : messages) {
            if (m.role == Role::System) {
                if (!system.empty()) system += "\n\n";
                system += m.content;
            } else {
                body["contents"].push_back({{"role", m.role == Role::Assistant ? "model" : "user"},
                                            {"parts", json::array({{{"text", m.content}}})}});
            }
        }
        if (!system.empty()) {
            body["systemInstruction"] = {{"parts", json::array({{{"text", system}}})}};
        }

        json generation = json::object();
        if (options.maxTokens) generation["maxOutputTokens"] = *options.maxTokens;
        if (options.temperature) generation["temperature"] = *options.temperature;
        if (options.topP) generation["topP"] = *options.topP;
        if (!options.stopSequences.empty()) generation["stopSequences"] = options.stopSequences;
        if (!generation.empty()) body["generationConfig"] = generation;

        const std::string url =
            joinEndpoint(config().baseUrl, "") + "/models/" + normalizeModel(modelFor(options)) + ":generateContent";
        const Headers headers{{"x-goog-api-key", config().apiKey}};
        const json result = postJson(url, headers, body);

        const auto& candidates = result.value("candidates", json::array());
        if (!candidates.is_array() || candidates.empty() || !candidates[0].is_object()) {
            std::string reason = "no candidates returned";
            if (result.contains("promptFeedback") && result["promptFeedback"].contains("blockReason")) {
                reason = "prompt blocked: " + result["promptFeedback"]["blockReason"].dump();
            }
            throw ParseError("gemini: " + reason);
        }
        const auto& candidate = candidates[0];

        Response response;
        if (candidate.contains("content") && candidate["content"].contains("parts") &&
            candidate["content"]["parts"].is_array()) {
            for (const auto& part : candidate["content"]["parts"]) {
                // Skip "thought" parts produced by thinking models.
                if (part.is_object() && part.contains("text") && part["text"].is_string() &&
                    !part.value("thought", false)) {
                    response.text += part["text"].get<std::string>();
                }
            }
        }
        if (candidate.contains("finishReason") && candidate["finishReason"].is_string()) {
            response.stopReason = candidate["finishReason"].get<std::string>();
        }
        response.model = result.value("modelVersion", normalizeModel(modelFor(options)));
        if (result.contains("usageMetadata") && result["usageMetadata"].is_object()) {
            const auto& usage = result["usageMetadata"];
            response.usage.inputTokens = usage.value("promptTokenCount", 0);
            // Thinking tokens are billed as output.
            response.usage.outputTokens = usage.value("candidatesTokenCount", 0) + usage.value("thoughtsTokenCount", 0);
        }
        response.rawJson = result.dump();
        return response;
    }
};

}  // namespace

std::unique_ptr<LLMService> makeGeminiService(ProviderConfig config, std::shared_ptr<HttpClient> http) {
    return std::make_unique<GeminiService>(std::move(config), std::move(http));
}

}  // namespace llmwrapper::detail
