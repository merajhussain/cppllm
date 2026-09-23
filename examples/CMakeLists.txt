add_executable(llmwrapper_cli cli/main.cpp)
target_link_libraries(llmwrapper_cli PRIVATE llmwrapper::llmwrapper)
if(MSVC)
    target_compile_options(llmwrapper_cli PRIVATE /W4 /permissive- /utf-8)
endif()

# Copy the DLL next to the exe when building shared, so it runs from the IDE.
if(WIN32 AND BUILD_SHARED_LIBS)
    add_custom_command(TARGET llmwrapper_cli POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:llmwrapper> $<TARGET_FILE_DIR:llmwrapper_cli>)
endif()
