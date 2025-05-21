cmake_minimum_required(VERSION 3.20)
project(learn)

set(CMAKE_CXX_STANDARD 20)

find_package(PkgConfig REQUIRED)
pkg_check_modules(PCAP REQUIRED libpcap)

add_executable(learn main.cpp)

target_include_directories(learn PRIVATE ${PCAP_INCLUDE_DIRS})
target_link_libraries(learn PRIVATE ${PCAP_LIBRARIES})
