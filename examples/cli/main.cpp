// Command-line demo of the library (replaces the Spring REST endpoint).
//
//   llmwrapper_cli [--env PATH] [--provider NAME] [--model NAME]
//                  [--system TEXT] [--max-tokens N] [--sentiment] PROMPT...
//
// Examples:
//   llmwrapper_cli --provider anthropic "Write a haiku about programming"
//   llmwrapper_cli --provider openai --sentiment "I love this new feature!"

#include <llmwrapper/llmwrapper.hpp>

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace {

void usage() {
    std::cerr << "usage: llmwrapper_cli [--env PATH] [--provider openai|anthropic|gemini|mistral]\n"
                 "                      [--model NAME] [--system TEXT] [--max-tokens N]\n"
                 "                      [--sentiment] PROMPT...\n";
}

}  // namespace

int main(int argc, char** argv) {
#if defined(_WIN32)
    SetConsoleOutputCP(CP_UTF8);  // show non-ASCII model output correctly
#endif
    std::string envPath = ".env";
    std::string providerName = "openai";
    std::string system;
    bool sentiment = false;
    llmwrapper::GenerationOptions options;
    std::string prompt;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto next = [&]() -> std::string {
            if (i + 1 >= argc) {
                usage();
                std::exit(2);
            }
            return argv[++i];
        };
        if (arg == "--env") envPath = next();
        else if (arg == "--provider") providerName = next();
        else if (arg == "--model") options.model = next();
        else if (arg == "--system") system = next();
        else if (arg == "--max-tokens") options.maxTokens = std::stoi(next());
        else if (arg == "--sentiment") sentiment = true;
        else if (arg == "-h" || arg == "--help") { usage(); return 0; }
        else prompt += (prompt.empty() ? "" : " ") + arg;
    }
    if (prompt.empty()) {
        usage();
        return 2;
    }

    try {
        auto client = llmwrapper::LLMClient::fromEnvironment(envPath);
        const auto provider = llmwrapper::parseProvider(providerName);

        if (sentiment) {
            std::cout << llmwrapper::toString(client.analyzeSentiment(provider, prompt, options)) << "\n";
            return 0;
        }

        std::vector<llmwrapper::Message> messages;
        if (!system.empty()) messages.push_back(llmwrapper::Message::system(system));
        messages.push_back(llmwrapper::Message::user(prompt));

        const auto response = client.chat(provider, messages, options);
        std::cout << response.text << "\n\n"
                  << "[" << response.model << " | stop: " << response.stopReason
                  << " | tokens in/out: " << response.usage.inputTokens << "/" << response.usage.outputTokens
                  << "]\n";
        return 0;
    } catch (const llmwrapper::ApiError& e) {
        std::cerr << e.what() << "\n";
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
}
