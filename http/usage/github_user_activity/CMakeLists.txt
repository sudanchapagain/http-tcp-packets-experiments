cmake_minimum_required(VERSION 3.20)
project(gua VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)

set(CMAKE_USE_OPENSSL ON)

FetchContent_MakeAvailable(nlohmann_json)

find_package(CURL REQUIRED)

add_executable(gua src/main.cpp)

target_link_libraries(gua
    PRIVATE
    nlohmann_json::nlohmann_json
)

