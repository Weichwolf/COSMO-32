cmake_minimum_required(VERSION 3.20)
project(cosmo32 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Optimierungen
option(ENABLE_LTO "Enable Link-Time Optimization" ON)
option(ENABLE_PGO_GEN "Enable PGO instrumentation" OFF)
option(ENABLE_PGO_USE "Enable PGO optimization" OFF)

set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -DNDEBUG")
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "")

if(ENABLE_LTO AND NOT ENABLE_PGO_GEN)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -flto")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -flto")
endif()

if(ENABLE_PGO_GEN)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fprofile-generate")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -fprofile-generate")
endif()

if(ENABLE_PGO_USE)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fprofile-use -fprofile-correction")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -fprofile-use")
endif()
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
)

if(WIN32)
    target_link_libraries(cosmo32 PRIVATE ws2_32)
endif()
