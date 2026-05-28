cmake_minimum_required(VERSION 3.20)

project(pacman_win
    VERSION 0.1.0
    DESCRIPTION "pacman for Windows"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

function(pacman_win_target_defaults target)
    target_include_directories(${target}
        PRIVATE
            ${PROJECT_SOURCE_DIR}
    )

    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

add_executable(pacman
    pacman/src/main.cpp
    pacman/src/switcher.cpp
)
pacman_win_target_defaults(pacman)

add_executable(langtools
    langtools/src/main.cpp
    langtools/src/core.cpp
)
pacman_win_target_defaults(langtools)
