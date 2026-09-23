# Integration tests: they call the real provider APIs. Keys come from a .env
# file (found in the source folder or its parents, or via LLMWRAPPER_ENV_FILE)
# or from environment variables. Providers without a key are skipped.
add_executable(llmwrapper_tests test_main.cpp)
target_link_libraries(llmwrapper_tests PRIVATE llmwrapper::llmwrapper)
if(MSVC)
    target_compile_options(llmwrapper_tests PRIVATE /W4 /permissive- /utf-8)
    target_compile_definitions(llmwrapper_tests PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
if(WIN32 AND BUILD_SHARED_LIBS)
    add_custom_command(TARGET llmwrapper_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:llmwrapper> $<TARGET_FILE_DIR:llmwrapper_tests>)
endif()

add_test(NAME llmwrapper_tests COMMAND llmwrapper_tests WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
set_tests_properties(llmwrapper_tests PROPERTIES
    SKIP_RETURN_CODE 77      # no API keys configured
    TIMEOUT 600
    LABELS integration)
