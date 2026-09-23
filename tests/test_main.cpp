// Integration tests: every provider test makes REAL API calls.
//
// API keys (and optional model overrides) are read from, in order:
//   1. --env <path> on the command line
//   2. the LLMWRAPPER_ENV_FILE environment variable
//   3. the first ".env" found in the working directory or up to 6 parent folders
//   4. process environment variables
// using the usual names: OPENAI_API_KEY, OPENAI_MODEL, ANTHROPIC_API_KEY,
// ANTHROPIC_MODEL, GEMINI_API_KEY, GEMINI_MODEL, MISTRAL_API_KEY, MISTRAL_MODEL.
//
// Providers without a key are SKIPPED, not failed.
// Usage: llmwrapper_tests [--env PATH] [--provider openai|anthropic|gemini|mistral]
//
// Each run sends ~10 short prompts per configured provider (a few hundred tokens).

#include <llmwrapper/llmwrapper.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <vector>

using namespace llmwrapper;
namespace fs = std::filesystem;

namespace {

// ------------------------------------------------------------------ tiny test harness

int g_failures = 0;
int g_checks = 0;

#define CHECK(cond)                                                                    \
    do {                                                                               \
        ++g_checks;                                                                    \
        if (!(cond)) {                                                                 \
            ++g_failures;                                                              \
            std::cerr << "    " << __FILE__ << ":" << __LINE__ << ": CHECK failed: " #cond "\n"; \
        }                                                                              \
    } while (0)

#define CHECK_MSG(cond, context)                                                       \
    do {                                                                               \
        ++g_checks;                                                                    \
        if (!(cond)) {                                                                 \
            ++g_failures;                                                              \
            std::cerr << "    " << __FILE__ << ":" << __LINE__ << ": CHECK failed: " #cond \
                      << "\n      got: " << (context) << "\n";                         \
        }                                                                              \
    } while (0)

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

bool contains(const std::string& haystack, const std::string& needle) {
    return lower(haystack).find(lower(needle)) != std::string::npos;
}

struct Stats {
    int passed = 0, failed = 0, skipped = 0;
};
Stats g_stats;

void run(const std::string& name, const std::function<void()>& fn) {
    const int before = g_failures;
    const auto start = std::chrono::steady_clock::now();
    try {
        fn();
    } catch (const ApiError& e) {
        ++g_failures;
        std::cerr << "    unexpected ApiError: " << e.what() << "\n";
    } catch (const std::exception& e) {
        ++g_failures;
        std::cerr << "    unexpected exception: " << e.what() << "\n";
    }
    const auto ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
    const bool ok = g_failures == before;
    (ok ? g_stats.passed : g_stats.failed)++;
    std::cout << (ok ? "[  OK  ] " : "[ FAIL ] ") << name << "  (" << ms << " ms)\n" << std::flush;
}

void skip(const std::string& name, const std::string& why) {
    ++g_stats.skipped;
    std::cout << "[ SKIP ] " << name << "  (" << why << ")\n";
}

// ------------------------------------------------------------------ configuration

std::optional<fs::path> findEnvFile(const std::string& explicitPath) {
    if (!explicitPath.empty()) return fs::path(explicitPath);
    if (const char* fromEnv = std::getenv("LLMWRAPPER_ENV_FILE"); fromEnv && *fromEnv) return fs::path(fromEnv);
    fs::path dir = fs::current_path();
    for (int i = 0; i <= 6; ++i) {
        if (fs::exists(dir / ".env")) return dir / ".env";
        if (!dir.has_parent_path() || dir.parent_path() == dir) break;
        dir = dir.parent_path();
    }
    return std::nullopt;
}

// ------------------------------------------------------------------ offline tests (no network, no mocks)

void offlineTests() {
    run("parseProvider", [] {
        CHECK(parseProvider("openai") == Provider::OpenAI);
        CHECK(parseProvider("Claude") == Provider::Anthropic);
        CHECK(parseProvider("GOOGLE") == Provider::Gemini);
        CHECK(parseProvider("mistral") == Provider::Mistral);
        bool threw = false;
        try { parseProvider("llama"); } catch (const ConfigError&) { threw = true; }
        CHECK(threw);
    });
    run("parseSentiment", [] {
        CHECK(parseSentiment("POSITIVE") == Sentiment::Positive);
        CHECK(parseSentiment(" negative.\n") == Sentiment::Negative);
        CHECK(parseSentiment("Neutral") == Sentiment::Neutral);
        CHECK(parseSentiment("no idea") == Sentiment::Unknown);
    });
    run("loadDotEnv", [] {
        const auto path = fs::temp_directory_path() / "llmwrapper_test.env";
        {
            std::ofstream f(path, std::ios::binary);
            f << "\xEF\xBB\xBF# comment\nA_KEY=plain\nexport B_KEY='quoted'\nC_KEY=value # note\n";
        }
        const auto v = loadDotEnv(path.string());
        CHECK(v.at("A_KEY") == "plain");
        CHECK(v.at("B_KEY") == "quoted");
        CHECK(v.at("C_KEY") == "value");
        fs::remove(path);
    });
    run("missing API key is a ConfigError", [] {
        bool threw = false;
        try { createService(Provider::OpenAI, ProviderConfig{}); } catch (const ConfigError&) { threw = true; }
        CHECK(threw);
    });
}

// ------------------------------------------------------------------ real network tests

void transportTests() {
    run("unreachable host -> TransportError", [] {
        ProviderConfig cfg;
        cfg.apiKey = "unused";
        cfg.baseUrl = "https://llmwrapper-test.invalid/v1";  // .invalid never resolves (RFC 2606)
        cfg.maxRetries = 0;
        cfg.timeout = std::chrono::seconds(15);
        bool threw = false;
        try {
            createService(Provider::OpenAI, cfg)->generateText("hi");
        } catch (const TransportError& e) {
            threw = true;
            CHECK(std::string(e.what()).find("llmwrapper-test.invalid") != std::string::npos);
        }
        CHECK(threw);
    });
}

bool isTruncationReason(const std::string& reason) {
    const auto r = lower(reason);
    return r == "length" || r == "max_tokens" || r == "model_length";
}

void providerTests(Provider provider, const ProviderConfig& cfg) {
    const std::string p = toString(provider);
    const auto svc = std::shared_ptr<LLMService>(createService(provider, cfg));

    run(p + ": generateText", [&] {
        const auto text = svc->generateText("What is capital of India, Reply in one word");
        CHECK_MSG(contains(text, "pong"), text);
    });

    run(p + ": response metadata (model, stop reason, token usage)", [&] {
        const auto r = svc->chat({Message::user("Say hello in one short sentence.")});
        CHECK_MSG(!r.text.empty(), r.rawJson);
        CHECK_MSG(!r.model.empty(), r.rawJson);
        CHECK_MSG(!r.stopReason.empty(), r.rawJson);
        CHECK_MSG(r.usage.inputTokens > 0, r.rawJson);
        CHECK_MSG(r.usage.outputTokens > 0, r.rawJson);
        CHECK(!r.rawJson.empty());
    });

    run(p + ": system prompt", [&] {
        const auto r = svc->chat({Message::system("You are a pirate. Every reply must contain the word 'arr'."),
                                  Message::user("Greet me in one short sentence.")});
        CHECK_MSG(contains(r.text, "arr"), r.text);
    });

    run(p + ": multi-turn history", [&] {
        const auto r = svc->chat({Message::user("My favourite colour is turquoise. Just say OK."),
                                  Message::assistant("OK."),
                                  Message::user("What is my favourite colour? Answer with one word.")});
        CHECK_MSG(contains(r.text, "turquoise"), r.text);
    });

    run(p + ": maxTokens truncates the answer", [&] {
        GenerationOptions opt;
        opt.maxTokens = 16;
        const auto r = svc->chat({Message::user("Write a 500-word essay about the history of the printing press.")}, opt);
        CHECK_MSG(isTruncationReason(r.stopReason), r.stopReason);
    });

    run(p + ": model override per call", [&] {
        GenerationOptions opt;
        opt.model = cfg.model.empty() ? defaultModel(provider) : cfg.model;
        const auto text = svc->generateText("Reply with exactly one word: ready", opt);
        CHECK_MSG(contains(text, "ready"), text);
    });

    run(p + ": analyzeSentiment", [&] {
        const auto pos = svc->analyzeSentiment("I absolutely love this product, it works beautifully!");
        const auto neg = svc->analyzeSentiment("This is terrible. It broke on day one and support ignored me.");
        CHECK_MSG(pos == Sentiment::Positive, toString(pos));
        CHECK_MSG(neg == Sentiment::Negative, toString(neg));
    });

    run(p + ": UTF-8 round trip", [&] {
        const auto text = svc->generateText(
            "Translate 'thank you' into Hindi. Reply with only the Hindi word(s) in Devanagari script.");
        const bool hasNonAscii =
            std::any_of(text.begin(), text.end(), [](char c) { return static_cast<unsigned char>(c) >= 0x80; });
        CHECK_MSG(hasNonAscii, text);
    });

    run(p + ": LLMClient + chatAsync (3 concurrent calls)", [&] {
        LLMClient client;
        client.addProvider(provider, cfg);
        std::vector<std::future<Response>> futures;
        for (int i = 1; i <= 3; ++i) {
            futures.push_back(client.chatAsync(
                provider, {Message::user("What is " + std::to_string(i) + " + " + std::to_string(i) +
                                         "? Reply with only the number.")}));
        }
        for (int i = 1; i <= 3; ++i) {
            const auto text = futures[static_cast<std::size_t>(i - 1)].get().text;
            CHECK_MSG(text.find(std::to_string(2 * i)) != std::string::npos, text);
        }
    });

    run(p + ": invalid API key -> ApiError", [&] {
        ProviderConfig bad = cfg;
        bad.apiKey = "invalid-key-for-llmwrapper-tests";
        bad.maxRetries = 0;
        bool threw = false;
        try {
            createService(provider, bad)->generateText("hi");
        } catch (const ApiError& e) {
            threw = true;
            // OpenAI/Anthropic/Mistral answer 401; Gemini answers 400 ("API key not valid") or 403.
            CHECK_MSG(e.status() == 400 || e.status() == 401 || e.status() == 403, e.what());
            CHECK(!e.isRetryable());
            CHECK(!e.body().empty());
        }
        CHECK(threw);
    });
}

}  // namespace

int main(int argc, char** argv) {
    std::string envArg;
    std::optional<Provider> only;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--env" && i + 1 < argc) envArg = argv[++i];
        else if (arg == "--provider" && i + 1 < argc) only = parseProvider(argv[++i]);
        else {
            std::cerr << "usage: llmwrapper_tests [--env PATH] [--provider NAME]\n";
            return 2;
        }
    }

    std::map<std::string, std::string> fileValues;
    if (const auto envFile = findEnvFile(envArg)) {
        if (!fs::exists(*envFile)) {
            std::cerr << "env file not found: " << envFile->string() << "\n";
            return 2;
        }
        fileValues = loadDotEnv(envFile->string());
        std::cout << "Using keys from " << fs::absolute(*envFile).string() << " (plus environment variables)\n\n";
    } else {
        std::cout << "No .env file found; using environment variables only\n\n";
    }

    offlineTests();
    transportTests();

    int configured = 0;
    for (Provider provider : {Provider::OpenAI, Provider::Anthropic, Provider::Gemini, Provider::Mistral}) {
        if (only && *only != provider) continue;
        const std::string name = toString(provider);
        const auto cfg = configFromEnvironment(provider, fileValues);
        if (!cfg) {
            std::string var = name;
            std::transform(var.begin(), var.end(), var.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
            skip(name + ": all tests", "no " + var + "_API_KEY");
            continue;
        }
        ++configured;
        std::cout << "\n--- " << name << " (model: " << (cfg->model.empty() ? defaultModel(provider) : cfg->model)
                  << ") ---\n";
        providerTests(provider, *cfg);
    }

    std::cout << "\n" << g_stats.passed << " passed, " << g_stats.failed << " failed, " << g_stats.skipped
              << " skipped (" << g_checks << " checks)\n";
    if (g_failures > 0) return 1;
    if (configured == 0) {
        std::cout << "No API keys configured: provider tests were skipped.\n";
        return 77;  // CTest reports this as "Skipped"
    }
    return 0;
}
