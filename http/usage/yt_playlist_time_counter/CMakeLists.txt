cmake_minimum_required(VERSION 3.16)
project(ytplaycount VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)

FetchContent_Declare(
    cpr
    GIT_REPOSITORY https://github.com/libcpr/cpr.git
    GIT_TAG 1.11.2
)

set(CPR_USE_SYSTEM_CURL OFF)
set(CMAKE_USE_OPENSSL ON)

FetchContent_MakeAvailable(nlohmann_json cpr)

add_executable(ytplaycount main.cpp)

target_link_libraries(ytplaycount
    PRIVATE
    cpr::cpr
    nlohmann_json::nlohmann_json
)

