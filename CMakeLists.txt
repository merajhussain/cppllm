cmake_minimum_required(VERSION 3.21)

project(llmwrapper
    VERSION 3.0.0
    DESCRIPTION "Unified C++ client for OpenAI, Anthropic, Google Gemini and Mistral"
    LANGUAGES CXX)

option(LLMWRAPPER_BUILD_EXAMPLES "Build the example CLI" ${PROJECT_IS_TOP_LEVEL})
option(LLMWRAPPER_BUILD_TESTS "Build the unit tests" ${PROJECT_IS_TOP_LEVEL})
option(LLMWRAPPER_USE_SYSTEM_JSON "Use an installed nlohmann_json instead of the bundled header" OFF)
option(LLMWRAPPER_INSTALL "Generate install rules" ${PROJECT_IS_TOP_LEVEL})
# HTTP transport: WinHTTP on Windows (built in, no dependencies), libcurl elsewhere.
if(WIN32)
    set(_llmwrapper_curl_default OFF)
else()
    set(_llmwrapper_curl_default ON)
endif()
option(LLMWRAPPER_USE_CURL "Use libcurl for HTTP (default: ON except on Windows)" ${_llmwrapper_curl_default})
# BUILD_SHARED_LIBS=ON builds llmwrapper.dll + import library instead of a static .lib.

# ---------------------------------------------------------------- library
add_library(llmwrapper
    src/types.cpp
    src/config.cpp
    src/llm_service.cpp
    src/llm_client.cpp
    src/provider_base.cpp
    src/providers/openai_compatible_service.cpp
    src/providers/anthropic_service.cpp
    src/providers/gemini_service.cpp
    src/http/winhttp_client.cpp
)
add_library(llmwrapper::llmwrapper ALIAS llmwrapper)

target_compile_features(llmwrapper PUBLIC cxx_std_17)

target_include_directories(llmwrapper
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)

# JSON is an implementation detail: it never appears in public headers.
if(LLMWRAPPER_USE_SYSTEM_JSON)
    find_package(nlohmann_json 3.11 REQUIRED)
    target_link_libraries(llmwrapper PRIVATE nlohmann_json::nlohmann_json)
else()
    target_include_directories(llmwrapper PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
endif()

if(BUILD_SHARED_LIBS)
    target_compile_definitions(llmwrapper PUBLIC LLMWRAPPER_SHARED PRIVATE LLMWRAPPER_BUILDING)
    set_target_properties(llmwrapper PROPERTIES CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON)
endif()

if(LLMWRAPPER_USE_CURL)
    find_package(CURL REQUIRED)
    target_sources(llmwrapper PRIVATE src/http/curl_client.cpp)
    target_link_libraries(llmwrapper PRIVATE CURL::libcurl)
    target_compile_definitions(llmwrapper PRIVATE LLMWRAPPER_USE_CURL)
    message(STATUS "llmwrapper: HTTP transport = libcurl ${CURL_VERSION_STRING}")
elseif(WIN32)
    target_link_libraries(llmwrapper PRIVATE winhttp)
    message(STATUS "llmwrapper: HTTP transport = WinHTTP")
else()
    message(WARNING "llmwrapper: no HTTP transport; you must supply your own llmwrapper::HttpClient")
endif()
if(WIN32)
    target_compile_definitions(llmwrapper PRIVATE _WIN32_WINNT=0x0A00 UNICODE _UNICODE)
endif()

find_package(Threads REQUIRED)
target_link_libraries(llmwrapper PUBLIC Threads::Threads)

if(MSVC)
    # /utf-8: sources and string literals are UTF-8. /permissive-: standard conformance.
    target_compile_options(llmwrapper PRIVATE /W4 /permissive- /utf-8 /Zc:__cplusplus /EHsc)
else()
    target_compile_options(llmwrapper PRIVATE -Wall -Wextra -Wpedantic)
endif()

set_target_properties(llmwrapper PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    DEBUG_POSTFIX d)

# ---------------------------------------------------------------- examples / tests
if(LLMWRAPPER_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(LLMWRAPPER_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ---------------------------------------------------------------- install / find_package
if(LLMWRAPPER_INSTALL)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)

    install(TARGETS llmwrapper
        EXPORT llmwrapperTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    install(DIRECTORY include/llmwrapper DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    if(MSVC AND BUILD_SHARED_LIBS)
        install(FILES $<TARGET_PDB_FILE:llmwrapper> DESTINATION ${CMAKE_INSTALL_BINDIR} OPTIONAL)
    endif()

    install(EXPORT llmwrapperTargets
        NAMESPACE llmwrapper::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/llmwrapper)

    configure_package_config_file(cmake/llmwrapperConfig.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/llmwrapperConfig.cmake
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/llmwrapper)
    write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/llmwrapperConfigVersion.cmake
        COMPATIBILITY SameMajorVersion)
    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/llmwrapperConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/llmwrapperConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/llmwrapper)
endif()
