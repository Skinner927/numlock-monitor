cmake_minimum_required(VERSION 3.10)
project(NumlockMonitor C)

set(CMAKE_C_STANDARD 99)

add_executable(NumlockMonitor WIN32 main.c resource.rc)
target_compile_definitions(NumlockMonitor PRIVATE
    _CRT_SECURE_NO_WARNINGS
)
target_link_libraries(NumlockMonitor PRIVATE Wtsapi32.lib)
target_compile_options(NumlockMonitor PRIVATE
    /W4            # Warning level 4 (highest)
    /WX            # Warnings as errors
    /permissive-   # Disable permissive mode (enforce standard compliance)
    /sdl           # Enable additional runtime checks (Security Development Lifecycle)
)

# .png to .ico converter: https://redketchup.io/icon-converter
