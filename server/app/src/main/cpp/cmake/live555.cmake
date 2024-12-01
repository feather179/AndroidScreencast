file(TO_CMAKE_PATH "${ANDROID_PROJECT_DIR}/../live555/live" LIVE555_ROOT_DIR)

file(GLOB LIVE555_SRC
        ${LIVE555_ROOT_DIR}/BasicUsageEnvironment/*.cpp
        ${LIVE555_ROOT_DIR}/groupsock/*.cpp
        ${LIVE555_ROOT_DIR}/groupsock/*.c
        ${LIVE555_ROOT_DIR}/UsageEnvironment/*.cpp
        ${LIVE555_ROOT_DIR}/liveMedia/*.cpp
        ${LIVE555_ROOT_DIR}/liveMedia/*.c
)

add_library(live555 STATIC
        ${LIVE555_SRC}
)

target_include_directories(live555 PUBLIC
        ${LIVE555_ROOT_DIR}/BasicUsageEnvironment/include
        ${LIVE555_ROOT_DIR}/groupsock/include
        ${LIVE555_ROOT_DIR}/UsageEnvironment/include
        ${LIVE555_ROOT_DIR}/liveMedia
        ${LIVE555_ROOT_DIR}/liveMedia/include
)

target_compile_definitions(live555 PUBLIC
        SOCKLEN_T=socklen_t
        _LARGEFILE_SOURCE=1
        _FILE_OFFSET_BITS=64
        NO_OPENSSL
#        DEBUG=1
)

target_compile_options(live555 PRIVATE
        -fdata-sections
        -ffunction-sections
        -flto
)
