cmake_minimum_required(VERSION 3.22.1)

project("live555" LANGUAGES C CXX)

file(GLOB LIVE555_SRC
        BasicUsageEnvironment/*.cpp
        groupsock/*.cpp
        groupsock/*.c
        UsageEnvironment/*.cpp
        liveMedia/*.cpp
        liveMedia/*.c
)

add_library(${PROJECT_NAME} STATIC
        ${LIVE555_SRC}
)

target_include_directories(${PROJECT_NAME} PUBLIC
        BasicUsageEnvironment/include
        groupsock/include
        UsageEnvironment/include
        liveMedia
        liveMedia/include
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
        SOCKLEN_T=socklen_t
        _LARGEFILE_SOURCE=1
        _FILE_OFFSET_BITS=64
        NO_OPENSSL
)
