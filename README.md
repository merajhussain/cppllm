# cppllm (C++)

A C++17 (supports C++ 23) library that gives your application one interface to **OpenAI, Anthropic Claude, Google Gemini and Mistral**. It's a port of the Java/Spring Boot `llmwrapper` service: the REST server is gone and you link the library straight into your app.

- **Cross-platform: Windows, Linux and macOS.** See [Dependencies](#dependencies) for what each platform uses.
- **Static `.lib` or `.dll`**, with a CMake package so other projects can use `find_package(llmwrapper)`.
- **Builds with Visual Studio 2026**: open `llmwrapper.sln` and build. CMake (Open Folder or a generated `.slnx`) also works.

## Dependencies

| Dependency | Used for | Windows | Linux / macOS | How you get it |
|---|---|---|---|---|
| [nlohmann/json](https://github.com/nlohmann/json) 3.12 (MIT) | Building requests and parsing responses | ✔ | ✔ | Bundled in `third_party/nlohmann/json.hpp`, so there's nothing to install. Set `-DLLMWRAPPER_USE_SYSTEM_JSON=ON` to use your own copy (e.g. from vcpkg). |
| WinHTTP | HTTPS requests | ✔ | — | Part of Windows. `winhttp.lib` is linked automatically. |
| libcurl | HTTPS requests | optional (`-DLLMWRAPPER_USE_CURL=ON`) | ✔ | Linux: `libcurl4-openssl-dev` / `libcurl-devel`. macOS: ships with the OS. |

The JSON library is used on **every** platform, Windows included. It's compiled into the library, and none of the public headers in `include/` include it, so applications that use llmwrapper don't need `nlohmann/json` on their include path.

## What's new compared with the Java version

| Area | Java service | C++ library |
|---|---|---|
| Sentiment analysis | Only Anthropic (the others were stubs) | All 4 providers; returns a `Sentiment` enum |
| Conversations | One prompt only | Multi-turn `Message` history, system prompts |
| Options | Hard-coded `max_tokens=1024` | `model`, `maxTokens`, `temperature`, `topP`, `stopSequences` for each call |
| Results | Plain string | `Response` with text, model, stop reason, token usage and raw JSON |
| Errors | `RuntimeException` | `ApiError` (HTTP status and body), `TransportError`, `ConfigError`, `ParseError` |
| Resilience | None | Retries 429, 5xx and network errors with exponential backoff |
| Mistral model | Not configurable | `MISTRAL_MODEL` |
| Async | None | `LLMClient::chatAsync()` returns a `std::future` |
| Clients | New OpenAI/Gemini client created on every call | One HTTP session shared by all providers, thread-safe |
| Config | Spring properties | Same `.env` variable names, or set them in code |

## Build on Windows with Visual Studio 2026

Install the **"Desktop development with C++"** workload.

### Option A: Open the solution (easiest)
1. Double-click **`llmwrapper.sln`**, or use File → Open → Project/Solution.
2. Choose **Debug** or **Release** and **x64** on the toolbar, then **Build → Build Solution** (Ctrl+Shift+B).
3. Press **F5** to run the tests. `llmwrapper_tests` is the startup project and makes real API calls; see [Tests](#tests).
4. To call a real model, right-click **llmwrapper_cli** → *Set as Startup Project*. Under *Properties → Debugging → Command Arguments*, enter something like `--env C:\llmwrapper\.env --provider anthropic "Hello"`.

Output goes to `out\msvc\x64\<Config>\`:

- `llmwrapper.lib` (Release) or `llmwrapperd.lib` (Debug)
- `llmwrapper_cli.exe`
- `llmwrapper_tests.exe`

The projects use Visual Studio's default toolset (`v145` in VS 2026) and the newest installed Windows SDK, so you don't need to retarget them.

**Using the `.lib` from your own Visual Studio project:**

- Add `C:\...\cpp\include` to *C/C++ → Additional Include Directories*.
- Add `llmwrapper.lib` (or `llmwrapperd.lib` for Debug) to *Linker → Input*.
- Set *C/C++ → Language → C++ Language Standard* to C++17 or newer.

`winhttp.lib` is linked automatically. You can also just add `vs\llmwrapper.vcxproj` to your solution and add a project reference to it.

### Option B: CMake, Open Folder
1. In VS 2026, go to **File → Open → Folder…** and pick the `cpp` folder.
2. Choose a preset from the toolbar: `x64 Debug (static lib)`, `x64 Release (static lib)`, `x64 Debug (DLL)` or `x64 Release (DLL)`.
3. Run **Build → Build All**. Use **Test → Run CTest** to run the tests.
4. To try it out, pick `llmwrapper_cli.exe` as the startup item.

### Option C: CMake, generate a `.slnx` solution
From a *Developer PowerShell for VS 2026*:
```powershell
cd C:\llmwrapper\cpp
cmake --preset vs2026                 # -> out\vs2026\llmwrapper.slnx
cmake --build --preset vs2026-release
ctest --preset vs2026-debug           # after building Debug
```
The `Visual Studio 18 2026` generator needs CMake 4.2 or newer. The CMake that comes with VS 2026 is new enough.

### Option D: CMake, command line with Ninja
```powershell
cmake --preset x64-release
cmake --build --preset x64-release
ctest --preset x64-release
cmake --install out/build/x64-release   # -> out/install/x64-release (include/, lib/, lib/cmake/)
```

Build outputs:

- Static: `llmwrapper.lib` (`llmwrapperd.lib` for Debug)
- DLL: `llmwrapper.dll` plus the `llmwrapper.lib` import library

## Build on Linux / macOS

You need a C++17 compiler, CMake 3.21 or newer, Ninja and the libcurl development package:

```bash
sudo apt install build-essential cmake ninja-build libcurl4-openssl-dev   # Debian/Ubuntu
sudo dnf install gcc-c++ cmake ninja-build libcurl-devel                  # Fedora/RHEL
brew install cmake ninja                                                  # macOS (libcurl ships with the OS)

cmake --preset unix-release
cmake --build --preset unix-release
ctest --preset unix-release
sudo cmake --install out/build/unix-release        # optional: /usr/local/{include,lib}
```

This produces `libllmwrapper.a`, or `libllmwrapper.so` / `.dylib` with the `unix-release-shared` preset. Proxies come from the standard `https_proxy` / `no_proxy` environment variables, and TLS uses the system CA store.

To use libcurl on Windows instead of WinHTTP (for example when you install it with vcpkg), configure with `-DLLMWRAPPER_USE_CURL=ON`.

## Using it in your C++ application

### 1. Add it to your CMake project
```cmake
# either vendor it:
add_subdirectory(path/to/llmwrapper/cpp)
# or, after `cmake --install`:
find_package(llmwrapper 3 REQUIRED)       # set CMAKE_PREFIX_PATH to the install dir

target_link_libraries(my_app PRIVATE llmwrapper::llmwrapper)
```
For a plain `.vcxproj` project, see "Using the `.lib` from your own Visual Studio project" above.

### 2. Call it
```cpp
#include <llmwrapper/llmwrapper.hpp>
#include <iostream>

int main() {
    using namespace llmwrapper;

    // Registers every provider with an API key in the environment or in .env
    auto client = LLMClient::fromEnvironment(".env");

    // Simplest call (what POST /api/llm/generate used to do)
    std::cout << client.generateText("anthropic", "Write a haiku about programming") << "\n";

    // Full control
    GenerationOptions opts;
    opts.maxTokens = 300;
    opts.temperature = 0.3;
    Response r = client.chat(Provider::OpenAI,
                             {Message::system("You are a concise assistant."),
                              Message::user("Explain quantum computing in one paragraph.")},
                             opts);
    std::cout << r.text << "\n(" << r.usage.inputTokens << " in / " << r.usage.outputTokens << " out)\n";

    // Sentiment
    if (client.analyzeSentiment(Provider::Gemini, "I love this new feature!") == Sentiment::Positive) { /* ... */ }

    // Async
    auto fut = client.chatAsync(Provider::Mistral, {Message::user("Bonjour!")});
    std::cout << fut.get().text << "\n";
}
```

### Configuring in code instead of `.env`
```cpp
ProviderConfig cfg;
cfg.apiKey = "sk-...";
cfg.model = "gpt-4o-mini";
cfg.timeout = std::chrono::seconds(30);
cfg.maxRetries = 3;
cfg.baseUrl = "https://my-proxy.example.com/v1";   // optional: proxies, Azure-style gateways, etc.

LLMClient client;
client.addProvider(Provider::OpenAI, cfg);

// or use a single provider directly
auto openai = createService(Provider::OpenAI, cfg);
```

### Error handling
```cpp
try {
    client.generateText(Provider::OpenAI, "hi");
} catch (const llmwrapper::ApiError& e) {      // provider returned 4xx/5xx
    std::cerr << e.status() << " " << e.what() << "\n" << e.body();
} catch (const llmwrapper::TransportError& e) { // DNS/TLS/timeout; e.systemCode() = GetLastError()
} catch (const llmwrapper::LLMError& e) {       // base class for everything else
}
```

## Configuration variables

These use the same names as the Java project's `.env` file, so you can reuse that file. Values in the `.env` file take precedence over process environment variables.

| Provider | API key | Model (default) | Endpoint override |
|---|---|---|---|
| OpenAI | `OPENAI_API_KEY` | `OPENAI_MODEL` (`gpt-4o-mini`) | `OPENAI_BASE_URL` |
| Anthropic | `ANTHROPIC_API_KEY` | `ANTHROPIC_MODEL` (`claude-sonnet-4-5`) | `ANTHROPIC_API_URL` |
| Gemini | `GEMINI_API_KEY` | `GEMINI_MODEL` (`gemini-2.5-flash`) | `GEMINI_BASE_URL` |
| Mistral | `MISTRAL_API_KEY` | `MISTRAL_MODEL` (`mistral-small-latest`) | `MISTRAL_BASE_URL` |

A provider with no API key is skipped. Calling a skipped provider throws `ConfigError`. Gemini accepts model names with or without the `models/` prefix.

## CLI example
```text
llmwrapper_cli --provider anthropic "Write a haiku about programming"
llmwrapper_cli --provider openai --model gpt-4o --system "Answer in French" "What is C++?"
llmwrapper_cli --provider gemini --sentiment "This product is amazing!"
```

## Project layout
```
cpp/
├── include/llmwrapper/     public headers (llmwrapper.hpp includes all of them)
├── src/                    implementation
│   ├── providers/          openai_compatible (OpenAI + Mistral), anthropic, gemini
│   └── http/               winhttp_client.cpp (Windows), curl_client.cpp (Linux/macOS)
├── third_party/nlohmann/   bundled JSON header (MIT)
├── examples/cli/           command-line demo
├── tests/                  integration tests (real API calls, keys from .env)
└── cmake/                  find_package support
```

## Tests

The tests call the **real APIs** and don't use mocks. For every provider that has a key, they check:

- plain text generation, system prompts and multi-turn history
- `maxTokens` truncation and per-call model overrides
- sentiment analysis
- UTF-8 text in both directions
- concurrent calls with `chatAsync`
- the returned metadata: model, stop reason and token usage
- a bad key producing a proper `ApiError`

They also make one network call to a host that doesn't exist, to check that it produces a `TransportError`.

**Configuring keys.** The tests use the same variables as everything else (`OPENAI_API_KEY`, `ANTHROPIC_API_KEY`, `GEMINI_API_KEY`, `MISTRAL_API_KEY`, plus the optional `*_MODEL`). They look for them in this order:

1. `--env C:\path\to\.env` on the command line
2. the `LLMWRAPPER_ENV_FILE` environment variable, set to the path of a `.env` file
3. the first `.env` file found in the working directory or any of the 6 folders above it. From `C:\llmwrapper\cpp` that finds `C:\llmwrapper\.env` automatically.
4. process environment variables

**Skipping providers.** A provider with no key is **skipped**, not failed. If no keys are configured at all, CTest reports the test as *Skipped*. To test a single provider, run `llmwrapper_tests --provider gemini`.

**Cost.** Each run sends about 12 short prompts per configured provider, a few hundred tokens in total.

## Extending
- **New provider:** add a class in `src/providers/` that derives from `detail::ProviderServiceBase`, then add a `Provider` enum value and a case in `createService()`.
- **Custom transport** (logging, a corporate HTTP stack, a mock for your own tests): implement `llmwrapper::HttpClient` and pass it to `LLMClient` or `createService`.

## CMake options
| Option | Default | |
|---|---|---|
| `BUILD_SHARED_LIBS` | `OFF` | Build a DLL instead of a static lib |
| `LLMWRAPPER_BUILD_EXAMPLES` | `ON` (top-level) | Build `llmwrapper_cli` |
| `LLMWRAPPER_BUILD_TESTS` | `ON` (top-level) | Build the unit tests |
| `LLMWRAPPER_USE_CURL` | `OFF` on Windows, `ON` elsewhere | Use libcurl for HTTP instead of WinHTTP |
| `LLMWRAPPER_USE_SYSTEM_JSON` | `OFF` | Use a `find_package(nlohmann_json)` (e.g. from vcpkg) instead of the bundled header |
| `LLMWRAPPER_INSTALL` | `ON` (top-level) | Generate install / `find_package` rules |
