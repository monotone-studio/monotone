#
# monotone cmake.
#
cmake_minimum_required(VERSION 3.24)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
set(CMAKE_SOURCE_DIR "${CMAKE_SOURCE_DIR}/..")

if ("${CMAKE_BUILD_TYPE}" STREQUAL "")
	set(CMAKE_BUILD_TYPE "Debug")
endif()

message(STATUS "")
message(STATUS "monotone.")
message(STATUS "")
message(STATUS "embeddable cloud-native storage for events and time-series data.")
message(STATUS "")
message(STATUS "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
message(STATUS "")

project(monotone C)

# compiler options
set(CMAKE_C_COMPILER "gcc")
if ("${CMAKE_BUILD_TYPE}" STREQUAL "Release")
	set(CMAKE_C_FLAGS "-Wall -Wextra -Wno-clobbered -fPIC -g -O2")
elseif("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
	set(CMAKE_C_FLAGS "-Wall -Wextra -Wno-clobbered -fPIC -g -O0")
endif()

set_property(GLOBAL PROPERTY TARGET_MESSAGES OFF)
set_property(GLOBAL PROPERTY RULE_MESSAGES OFF)
set(CMAKE_C_COMPILER_LAUNCHER "${CMAKE_SOURCE_DIR}/build/launcher.sh")
set(CMAKE_C_LINKER_LAUNCHER "${CMAKE_SOURCE_DIR}/build/launcher.sh")

# libraries
set(CMAKE_THREAD_PREFER_PTHREAD true)

find_package(Threads REQUIRED)
find_package(CURL REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(ZSTD REQUIRED)
find_package(LZ4 REQUIRED)

include_directories(${CURL_INCLUDE_DIR})
include_directories(${OPENSSL_INCLUDE_DIR})
include_directories(${ZSTD_INCLUDE_DIR})
include_directories(${LZ4_INCLUDE_DIR})

set(MONOTONE_LIBRARIES m
    ${CMAKE_THREAD_LIBS_INIT}
    ${CURL_LIBRARY}
    ${OPENSSL_LIBRARIES}
    ${ZSTD_LIBRARIES}
    ${LZ4_LIBRARIES})

# project includes
set(MONOTONE_ROOT "${CMAKE_SOURCE_DIR}/monotone")

include_directories("${MONOTONE_ROOT}/runtime")
include_directories("${MONOTONE_ROOT}/runtime/runtime")
include_directories("${MONOTONE_ROOT}/runtime/lib")
include_directories("${MONOTONE_ROOT}/runtime/config")
include_directories("${MONOTONE_ROOT}/cloud")
include_directories("${MONOTONE_ROOT}/cloud/cloud")
include_directories("${MONOTONE_ROOT}/cloud/s3")
include_directories("${MONOTONE_ROOT}/engine")
include_directories("${MONOTONE_ROOT}/engine/io")
include_directories("${MONOTONE_ROOT}/engine/storage")
include_directories("${MONOTONE_ROOT}/engine/wal")
include_directories("${MONOTONE_ROOT}/engine/engine")
include_directories("${MONOTONE_ROOT}/main")
include_directories("${MONOTONE_ROOT}/main/command")
include_directories("${MONOTONE_ROOT}/main/main")
include_directories("${MONOTONE_ROOT}/main/api")
include_directories("${CMAKE_SOURCE_DIR}/test")
include_directories("${CMAKE_SOURCE_DIR}/test/suite")

# sources
set(MONOTONE_SOURCES_RUNTIME
    ${MONOTONE_ROOT}/runtime/runtime/runtime.c
    ${MONOTONE_ROOT}/runtime/runtime/vfs.c
    ${MONOTONE_ROOT}/runtime/runtime/logger.c
    ${MONOTONE_ROOT}/runtime/lib/json.c
    ${MONOTONE_ROOT}/runtime/lib/rbtree.c
    ${MONOTONE_ROOT}/runtime/lib/random.c
    ${MONOTONE_ROOT}/runtime/lib/uuid.c
    ${MONOTONE_ROOT}/runtime/lib/crc.c
    ${MONOTONE_ROOT}/runtime/lib/compression_lz4.c
    ${MONOTONE_ROOT}/runtime/lib/compression_zstd.c
    ${MONOTONE_ROOT}/runtime/lib/compression_mgr.c
    ${MONOTONE_ROOT}/runtime/lib/encryption_aes.c
    ${MONOTONE_ROOT}/runtime/lib/encryption_mgr.c
    ${MONOTONE_ROOT}/runtime/config/config.c)

set(MONOTONE_SOURCES_CLOUD
    ${MONOTONE_ROOT}/cloud/cloud/cloud_mgr.c
    ${MONOTONE_ROOT}/cloud/cloud/mock.c
    ${MONOTONE_ROOT}/cloud/s3/s3_io.c
    ${MONOTONE_ROOT}/cloud/s3/s3_request.c
    ${MONOTONE_ROOT}/cloud/s3/s3_op.c
    ${MONOTONE_ROOT}/cloud/s3/s3.c)

set(MONOTONE_SOURCES_ENGINE
    ${MONOTONE_ROOT}/engine/io/memtable.c
    ${MONOTONE_ROOT}/engine/io/index_op.c
    ${MONOTONE_ROOT}/engine/io/part.c
    ${MONOTONE_ROOT}/engine/io/writer.c
    ${MONOTONE_ROOT}/engine/io/merger.c
    ${MONOTONE_ROOT}/engine/storage/storage_mgr.c
    ${MONOTONE_ROOT}/engine/storage/pipeline.c
    ${MONOTONE_ROOT}/engine/wal/wal.c
    ${MONOTONE_ROOT}/engine/wal/wal_cursor.c
    ${MONOTONE_ROOT}/engine/engine/engine.c
    ${MONOTONE_ROOT}/engine/engine/engine_recover.c
    ${MONOTONE_ROOT}/engine/engine/engine_show.c
    ${MONOTONE_ROOT}/engine/engine/write.c
    ${MONOTONE_ROOT}/engine/engine/refresh.c
    ${MONOTONE_ROOT}/engine/engine/op.c
    ${MONOTONE_ROOT}/engine/engine/worker.c
    ${MONOTONE_ROOT}/engine/engine/worker_mgr.c)

set(MONOTONE_SOURCES_MAIN
    ${MONOTONE_ROOT}/main/command/lex.c
    ${MONOTONE_ROOT}/main/command/parse_config.c
    ${MONOTONE_ROOT}/main/command/parse_cloud.c
    ${MONOTONE_ROOT}/main/command/parse_storage.c
    ${MONOTONE_ROOT}/main/command/parse_pipeline.c
    ${MONOTONE_ROOT}/main/command/parse_partition.c
    ${MONOTONE_ROOT}/main/command/parse_debug.c
    ${MONOTONE_ROOT}/main/command/parse.c
    ${MONOTONE_ROOT}/main/main/main.c
    ${MONOTONE_ROOT}/main/main/execute.c
    ${MONOTONE_ROOT}/main/api/monotone.c)

set(MONOTONE_SOURCES
    ${MONOTONE_SOURCES_RUNTIME}
    ${MONOTONE_SOURCES_CLOUD}
    ${MONOTONE_SOURCES_ENGINE}
    ${MONOTONE_SOURCES_MAIN})

# static library
add_library(monotone_static STATIC ${MONOTONE_SOURCES})
set_target_properties(monotone_static PROPERTIES OUTPUT_NAME monotone)
set(MONOTONE_STATIC_LINK "$<LINK_LIBRARY:WHOLE_ARCHIVE,monotone_static>")

# shared library
file(TOUCH libmonotone_so.c)	
add_library(monotone SHARED libmonotone_so.c)
set_target_properties(monotone PROPERTIES OUTPUT_NAME monotone)
target_link_libraries(monotone ${MONOTONE_STATIC_LINK})
target_link_libraries(monotone ${MONOTONE_LIBRARIES})

# cli
add_executable(monotone_cli
               ${CMAKE_SOURCE_DIR}/cli/monotone.c
               ${CMAKE_SOURCE_DIR}/cli/bench.c)
target_link_libraries(monotone_cli ${MONOTONE_STATIC_LINK})
target_link_libraries(monotone_cli ${MONOTONE_LIBRARIES})
set_target_properties(monotone_cli PROPERTIES OUTPUT_NAME monotone)

# test suite
add_executable(monotone_test
               ${CMAKE_SOURCE_DIR}/test/suite/suite.c
               ${CMAKE_SOURCE_DIR}/test/suite/suite_cmd.c
               ${CMAKE_SOURCE_DIR}/test/test.c)
target_link_libraries(monotone_test ${MONOTONE_STATIC_LINK})
target_link_libraries(monotone_test ${MONOTONE_LIBRARIES})
set_target_properties(monotone_test PROPERTIES OUTPUT_NAME monotone-test)

# examples
add_executable(example_write_and_read EXCLUDE_FROM_ALL
               ${CMAKE_SOURCE_DIR}/examples/write_and_read.c)
target_link_libraries(example_write_and_read ${MONOTONE_STATIC_LINK})
target_link_libraries(example_write_and_read ${MONOTONE_LIBRARIES})
set_target_properties(example_write_and_read PROPERTIES OUTPUT_NAME example_write_and_read)
add_custom_target(examples DEPENDS example_write_and_read)
