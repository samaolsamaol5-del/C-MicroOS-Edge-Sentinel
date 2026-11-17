cmake_minimum_required(VERSION 3.10)
project(CMicroOS C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(CURL REQUIRED)

include_directories(include)

add_executable(cmicro_os
    src/main.c
    src/os_pulse_core.c
    src/web_server.c
)

target_link_libraries(cmicro_os CURL::libcurl)

if(WIN32)
    target_link_libraries(cmicro_os ws2_32 psapi)
endif()
