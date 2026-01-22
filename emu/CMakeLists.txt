cmake_minimum_required(VERSION 3.20)
project(cosmo32 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Optimierungen
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -fsanitize=address,undefined")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O3 -march=native -g -pg")  # For gprof

# SDL2 - ohne SDL2main (wir nutzen SDL_MAIN_HANDLED)
find_package(SDL2 REQUIRED CONFIG)

# Sources
file(GLOB_RECURSE SOURCES src/*.cpp)

add_executable(cosmo32 ${SOURCES})

target_include_directories(cosmo32 PRIVATE
    src
)

target_link_libraries(cosmo32 PRIVATE
    SDL2::SDL2
    ws2_32  # Windows sockets für UDP/TCP proxy
)
