cmake_minimum_required(VERSION 3.13)
project(ghostwriter VERSION 0.1)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# OPTIONS
option(GHOSTWRITER_BUILD_TESTS "Set ON if tests should be built." OFF)
option(GHOSTWRITER_BUILD_NETWORK_TESTS "Set ON if test client/server should be built." ON)
option(GHOSTWRITER_BUILD_STORAGE
       "Set ON if Ghostwriter storage should be built." OFF)
option(GHOSTWRITER_BUILD_BROKER
       "Set ON if Ghostwriter broker should be built." ON)
option(GHOSTWRITER_BUILD_MICROBENCHMARKS
       "Set ON if micro-benchmarks should be built." ON)
option(GHOSTWRITER_BUILD_KAFKA_MICROBENCHMARKS
       "Set ON if Kafka micro-benchmarks should be built" ON)
option(GHOSTWRITER_BUILD_YSB_BENCHMARK
       "Set ON if YSB benchmarks should be built." ON)
option(GHOSTWRITER_BUILD_YSB_KAFKA_BENCHMARK
       "Set ON if YSB Kafka benchmarks should be built." ON)
option(GHOSTWRITER_BUILD_YSB_LOCAL_RUNNER
       "Set ON if YSB local runner should be built." OFF)
option(GHOSTWRITER_BUILD_YSB_DATA_GENERATOR
       "Set ON if the external YSB data generator should be built." OFF)

set(GHOSTWRITER_BOOST_PATH
  "/scratch/hendrik.makait/usr"
  CACHE STRING "Path to custom Boost install directory")

set(GHOSTWRITER_GTEST_PATH
  "/scratch/hendrik.makait/usr"
  CACHE STRING "Path to custom googletest install directory")

set(GHOSTWRITER_JEMALLOC_PATH
  "/scratch/hendrik.makait/usr"
  CACHE STRING "Path to custom jemalloc install directory")

set(GHOSTWRITER_LLVM_PATH
  "/scratch/hendrik.makait/llvm-project"
  CACHE STRING "Path to custom LLVM install directory")

set(GHOSTWRITER_PMDK_PATH
    "/usr/local"
    CACHE STRING "Path to custom PMDK install directory")

set(GHOSTWRITER_UCX_PATH
  "/scratch/hendrik.makait/usr"
  CACHE STRING "Path to custom UCX install directory")

set(benchmark_DIR "/scratch/hendrik.makait/benchmark/build")

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pthread") # -lnuma")
set(CMAKE_CXX_FLAGS
    "${CMAKE_CXX_FLAGS} -std=c++17 -Wall -Wextra -DHAVE_NUM -DPREFETCH")

include_directories(${GHOSTWRITER_BOOST_PATH}/include)

set(GTEST_ROOT ${GHOSTWRITER_GTEST_PATH})
find_package(TBB REQUIRED tbb tbbmalloc)
if (NOT TBB_FOUND)
  message(FATAL ERROR "TBB not found in default locations or custom path ${GHOSTWRITER_TBB_PATH}")
else()
  message(STATUS "TBB found. Linking ${TBB_LIBRARIES} and including ${TBB_INCLUDE_DIRS}")
endif()
set(TBB_LIBRARIES TBB::tbb)
message("Build type: ${CMAKE_BUILD_TYPE}")
if(CMAKE_BUILD_TYPE MATCHES Debug)
  message("BUILDING DEBUG")
  add_compile_options(-g)
else()
  message("BUILDING RELEASE")
  add_compile_options(-O3 -march=native -mtune=native -mavx)
endif()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

set(BOOST_ROOT ${GHOSTWRITER_BOOST_PATH})
set(BOOST_LIBRARYDIR ${BOOST_ROOT}/lib)
set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)

# Temporarily remove custom cmake module path so the vendored FindBoost.cmake
# does not shadow modern Boost's own CMake config files.
set(_SAVED_CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH})
set(CMAKE_MODULE_PATH "")
find_package(
  Boost 1.68.0
  REQUIRED
  COMPONENTS program_options)
set(CMAKE_MODULE_PATH ${_SAVED_CMAKE_MODULE_PATH})

if(Boost_FOUND)
  include_directories(${Boost_INCLUDE_DIRS})
else()
  message(FATAL_ERROR "Boost not found in default locations or custom path (${BOOST_ROOT}).")
endif()

add_subdirectory(extern/librdkafka EXCLUDE_FROM_ALL)
add_subdirectory(extern/hdrhistogram EXCLUDE_FROM_ALL)


# UCX
set(UCX_INCLUDE_PATHS ${GHOSTWRITER_UCX_PATH}/include)
set(UCX_LIB_PATHS ${GHOSTWRITER_UCX_PATH}/lib)

find_library(
  UCP_LIBRARIES
  NAMES ucp libucp
  PATHS ${UCX_LIB_PATHS})
find_library(
  UCS_LIBRARIES
  NAMES ucs libucs
  PATHS ${UCX_LIB_PATHS})
find_library(
  UCT_LIBRARIES
  NAMES uct libuct
  PATHS ${UCX_LIB_PATHS})
find_path(
  UCX_INCLUDE_DIRS
  NAMES ucp/api/ucp.h
  PATHS ${UCX_INCLUDE_PATHS})

set(UCX_LIBRARIES ${UCP_LIBRARIES} ${UCS_LIBRARIES} ${UCT_LIBRARIES})

if(NOT UCX_INCLUDE_DIRS OR "${UCX_INCLUDE_DIRS}" STREQUAL "")
  message(
    FATAL_ERROR
      "UCX include directory not found in default locations or custom path (${UCX_INCLUDE_PATHS})."
  )
endif()
if(NOT UCP_LIBRARIES OR "${UCP_LIBRARIES}" STREQUAL "")
  message(
    FATAL_ERROR
      "UCP not found in default locations or custom path (${UCX_LIB_PATHS}).")
endif()
if(NOT UCS_LIBRARIES OR "${UCS_LIBRARIES}" STREQUAL "")
  message(
    FATAL_ERROR
      "UCS not found in default locations or custom path (${UCX_LIB_PATHS}).")
endif()
if(NOT UCT_LIBRARIES OR "${UCT_LIBRARIES}" STREQUAL "")
  message(
    FATAL_ERROR
      "UCT not found in default locations or custom path (${UCX_LIB_PATHS}).")
endif()
message(
  STATUS "Found UCX. Including ${UCX_INCLUDE_DIRS} and linking ${UCX_LIBRARIES}"
)

# JEMALLOC
set(JEMALLOC_LIB_PATHS ${GHOSTWRITER_JEMALLOC_PATH}/lib)
find_library(JEMALLOC_LIBRARIES jemalloc PATHS ${JEMALLOC_LIB_PATHS})
if(NOT JEMALLOC_LIBRARIES)
  message(FATAL_ERROR "jemalloc not found in default locations or custom path (${JEMALLOC_LIB_PATHS}).")
endif()
message(STATUS "Found jemalloc. Linking ${JEMALLOC_LIBRARIES}")

# FLATBUFFERS
set(FLATBUFFERS_SRC_DIR ${PROJECT_SOURCE_DIR}/extern/flatbuffers)
# Add FlatBuffers directly to our build. This defines the `flatbuffers` target.
add_subdirectory(${FLATBUFFERS_SRC_DIR}
                 ${CMAKE_CURRENT_BINARY_DIR}/flatbuffers-build EXCLUDE_FROM_ALL)
# Now simply link against flatbuffers as needed to your already declared target.
# The flatbuffers target carry header search path automatically if CMake >
# 2.8.11. target_link_libraries(own_project_target PRIVATE flatbuffers)


# add LLVM libraries, set LLVM_* variables (only needed for YSB builds)
if(${GHOSTWRITER_BUILD_YSB_BENCHMARK} OR ${GHOSTWRITER_BUILD_YSB_LOCAL_RUNNER} OR ${GHOSTWRITER_BUILD_YSB_KAFKA_BENCHMARK})
  set(ENV{LLVM_HOME} "${GHOSTWRITER_LLVM_PATH}/build")
  message(STATUS "LLVM_HOME: $ENV{LLVM_HOME}")
  set(ENV{PATH} "$ENV{LLVM_HOME}/bin:$ENV{PATH}")
  set(LLVM_BUILD_DIR ${GHOSTWRITER_LLVM_PATH}/build)
  find_package(LLVM 9.0 REQUIRED PATHS ${LLVM_HOME}../../)
  if(NOT LLVM_FOUND)
    message(FATAL_ERROR "LLVM not found in default locations or custom path (${GHOSTWRITER_LLVM_PATH}).")
  endif()
  message(STATUS "Found LLVM v${LLVM_PACKAGE_VERSION}")
  # add Clang libraries
  include(${LLVM_BUILD_DIR}/lib/cmake/clang/ClangTargets.cmake)

  # find Clang source directory
  if (EXISTS ${LLVM_BUILD_MAIN_SRC_DIR}/tools/clang)
      set(CLANG_SRC_DIR_PREFIX tools) # sources in-tree (default build)
  elseif (EXISTS ${LLVM_BUILD_MAIN_SRC_DIR}/../clang)
      set(CLANG_SRC_DIR_PREFIX ..) # sources out-of-tree (ENABLE_PROJECTS build)
  else ()
      message(FATAL_ERROR "Cannot find Clang sources")
  endif ()
endif()


set(STORAGE_FILES
    src/storage/persistent_storage_region.cpp src/storage/storage_manager.cpp
    src/storage/storage_node.cpp src/storage/volatile_storage_region.cpp)

set(BROKER_FILES src/broker/broker_node.cpp src/broker/partition.cpp
                 src/broker/logical_segment.cpp src/broker/physical_segment.cpp)

set(CONSUMER_FILES
    src/broker/partition.cpp src/broker/logical_segment.cpp
    src/broker/physical_segment.cpp src/consumer/direct_consumer.cpp
    src/consumer/receiver.cpp)

set(PRODUCER_FILES
    src/broker/partition.cpp
    src/broker/logical_segment.cpp
    src/broker/physical_segment.cpp
    src/producer/batch.cpp
    src/producer/direct_producer.cpp
    src/producer/message_accumulator.cpp
    src/producer/sender.cpp)

set(COMMON_FILES
    src/network/attached_message.cpp
    src/network/client.cpp
    src/network/detached_message.cpp
    src/network/flat_buffers_message.cpp
    src/network/connection_manager.cpp
    src/network/message_handler.cpp
    src/network/request_processor.cpp
    src/network/ucx/context.cpp
    src/network/ucx/endpoint.cpp
    src/network/ucx/endpoint_factory.cpp
    src/network/ucx/memory_region.cpp
    src/network/ucx/worker.cpp
    src/network/server.cpp
    src/network/utils.cpp
    src/protocol/message_generator.cpp
    src/protocol/protocol.cpp
    src/storage/segment.cpp)


if (${GHOSTWRITER_BUILD_STORAGE})
# PMDK
set(PMDK_INCLUDE_HINTS ${GHOSTWRITER_PMDK_PATH}/include)
set(PMDK_LIB_HINTS ${GHOSTWRITER_PMDK_PATH}/lib)

find_path(PMDK_INCLUDE_DIRS libpmem.h HINTS ${PMDK_INCLUDE_HINTS})
find_library(
  PMDK_LIBRARIES
  NAMES pmem libpmem
  HINTS ${PMDK_LIB_HINTS})
find_library(
  PMEMOBJ_LIBRARIES
  NAMES pmemobj libpmemobj
  HINTS ${PMDK_LIB_HINTS})
endif()

if(${GHOSTWRITER_BUILD_STORAGE} AND (NOT PMDK_INCLUDE_DIRS OR "${PMDK_INCLUDE_DIRS}" STREQUAL ""))
  message(
    FATAL_ERROR
      "libpmem include directory not found in default locations or custom path (${PMDK_INCLUDE_HINTS})."
  )
endif()
if(${GHOSTWRITER_BUILD_STORAGE} AND (NOT PMDK_LIBRARIES OR "${PMDK_LIBRARIES}" STREQUAL ""))
  message(
    FATAL_ERROR
      "libmem not found in default locations or custom path (${PMDK_LIB_HINTS})."
  )
endif()
if(${GHOSTWRITER_BUILD_STORAGE} AND (NOT PMEMOBJ_LIBRARIES OR "${PMEMOBJ_LIBRARIES}" STREQUAL ""))
  message(
    FATAL_ERROR
      "libmemobj not found in default locations or custom path (${PMDK_LIB_HINTS})."
  )
endif()
if(${GHOSTWRITER_BUILD_STORAGE})
message(
  STATUS
    "Found PMDK. Including ${PMDK_INCLUDE_DIRS} and linking ${PMDK_LIBRARIES}, ${PMEMOBJ_LIBRARIES}"
)
endif()

if (${GHOSTWRITER_BUILD_STORAGE})
# BENCHMARK STORAGE
add_executable(
  benchmark_storage_node
  src/benchmark/common/ghostwriter/benchmark_storage_node.cpp
  src/storage/persistent_storage_region.cpp ${COMMON_FILES} ${STORAGE_FILES}
  ${HEADER_LIST})
target_link_libraries(
  benchmark_storage_node
  ${UCX_LIBRARIES}
  flatbuffers
  ${JEMALLOC_LIBRIES}
  ${Boost_LIBRARIES}
  ${TBB_LIBRARIES}
  hdr_histogram)
target_link_libraries(benchmark_storage_node ${PMDK_LIBRARIES}
                      ${PMEMOBJ_LIBRARIES})
target_include_directories(benchmark_storage_node PUBLIC include)
target_include_directories(
  benchmark_storage_node PRIVATE ${UCX_INCLUDE_DIRS} ${Boost_INCLUDE_DIRS}
                                 ${TBB_INCLUDE_DIRS})
target_include_directories(benchmark_storage_node PRIVATE ${PMDK_INCLUDE_DIRS})
endif()

if (${GHOSTWRITER_BUILD_BROKER})
# BENCHMARK BROKER
add_executable(
  benchmark_broker_node
  src/benchmark/common/ghostwriter/benchmark_broker_node.cpp ${COMMON_FILES}
  ${BROKER_FILES} ${HEADER_LIST})
target_link_libraries(benchmark_broker_node ${UCX_LIBRARIES} flatbuffers
                      ${Boost_LIBRARIES})
target_include_directories(benchmark_broker_node PUBLIC include)
target_include_directories(
  benchmark_broker_node PRIVATE ${UCX_INCLUDE_DIRS} ${Boost_INCLUDE_DIRS}
                                ${TBB_INCLUDE_DIRS})
endif()

if(${GHOSTWRITER_BUILD_MICROBENCHMARKS})
  set(MICROBENCHMARKS_SOURCE_FILES
      src/benchmark/common/data_generator.cpp
      src/benchmark/common/parallel_data_generator.cpp
      src/benchmark/common/data_processor.cpp
      src/benchmark/common/parallel_data_processor.cpp
      src/benchmark/common/rate_limiter.cpp
      src/benchmark/common/throughput_throttler.cpp
      src/logging/latency_logger.cpp
      src/logging/windowed_latency_logger.cpp
      src/logging/throughput_logger.cpp)

  # MICROBENCHMARKS PRODUCER
  add_executable(
    benchmark_producer
    src/benchmark/throughput/ghostwriter/producer.cpp ${COMMON_FILES}
    ${PRODUCER_FILES} ${MICROBENCHMARKS_SOURCE_FILES} ${HEADER_LIST})
  target_link_libraries(benchmark_producer ${UCX_LIBRARIES} flatbuffers
                        ${Boost_LIBRARIES} ${TBB_LIBRARIES} hdr_histogram)
  target_include_directories(benchmark_producer PUBLIC include)
  target_include_directories(
    benchmark_producer PRIVATE ${UCX_INCLUDE_DIRS} ${Boost_INCLUDE_DIRS}
                               ${TBB_INCLUDE_DIRS})

  # MICROBENCHMARKS CONSUMER
  add_executable(
    benchmark_consumer
    src/benchmark/throughput/ghostwriter/consumer.cpp ${COMMON_FILES}
    ${CONSUMER_FILES} ${MICROBENCHMARKS_SOURCE_FILES} ${HEADER_LIST})
  target_link_libraries(benchmark_consumer ${UCX_LIBRARIES} flatbuffers
                        ${Boost_LIBRARIES} ${TBB_LIBRARIES} hdr_histogram)
  target_include_directories(benchmark_consumer PUBLIC include)
  target_include_directories(
    benchmark_consumer PRIVATE ${UCX_INCLUDE_DIRS} ${Boost_INCLUDE_DIRS}
                               ${TBB_INCLUDE_DIRS})
endif()

if(${GHOSTWRITER_BUILD_KAFKA_MICROBENCHMARKS})
  set(KAFKA_MICROBENCHMARKS_SOURCE_FILES
      src/benchmark/common/data_generator.cpp
      src/benchmark/common/parallel_data_generator.cpp
      src/benchmark/common/kafka/data_processor.cpp
      src/benchmark/common/kafka/parallel_data_processor.cpp
      src/benchmark/common/rate_limiter.cpp
      src/benchmark/common/throughput_throttler.cpp
      src/logging/latency_logger.cpp
      src/logging/windowed_latency_logger.cpp
      src/logging/throughput_logger.cpp)
  
  set(RDKAFKA_BUILD_STATIC ON CACHE BOOL "")

  # KAFKA MICROBENCHMARKS PRODUCER
  add_executable(
    kafka_benchmark_producer
    src/benchmark/throughput/kafka/producer.cpp 
    ${KAFKA_MICROBENCHMARKS_SOURCE_FILES} ${HEADER_LIST})
  target_link_libraries(kafka_benchmark_producer flatbuffers
                        ${Boost_LIBRARIES} ${TBB_LIBRARIES} hdr_histogram rdkafka++)
  target_include_directories(kafka_benchmark_producer PUBLIC include)
  target_include_directories(
    kafka_benchmark_producer PRIVATE ${Boost_INCLUDE_DIRS}
                               ${TBB_INCLUDE_DIRS})

  # KAFKA MICROBENCHMARKS CONSUMER
  add_executable(
    kafka_benchmark_consumer
    src/benchmark/throughput/kafka/consumer.cpp ${KAFKA_MICROBENCHMARKS_SOURCE_FILES} ${HEADER_LIST})
  target_link_libraries(kafka_benchmark_consumer flatbuffers
                        ${Boost_LIBRARIES} ${TBB_LIBRARIES} hdr_histogram rdkafka++)
  target_include_directories(kafka_benchmark_consumer PUBLIC include)
  target_include_directories(
    kafka_benchmark_consumer PRIVATE ${Boost_INCLUDE_DIRS}
                               ${TBB_INCLUDE_DIRS})
endif()

if(${GHOSTWRITER_BUILD_YSB_BENCHMARK} OR ${GHOSTWRITER_BUILD_YSB_LOCAL_RUNNER} OR ${GHOSTWRITER_BUILD_YSB_KAFKA_BENCHMARK})
  add_subdirectory(extern/lightsaber)
  set(LIGHTSABER_FILES
      extern/lightsaber/src/dispatcher/TaskDispatcher.cpp
      extern/lightsaber/src/monitors/PerformanceMonitor.cpp
      extern/lightsaber/src/monitors/Measurement.cpp
      extern/lightsaber/src/monitors/LatencyMonitor.cpp
      extern/lightsaber/src/processor/TaskProcessor.cpp
      extern/lightsaber/src/result/ResultHandler.cpp
      extern/lightsaber/src/tasks/NumaTaskQueueWrapper.cpp
      extern/lightsaber/src/tasks/WindowBatch.cpp
      extern/lightsaber/src/tasks/Task.cpp
      extern/lightsaber/src/utils/AttributeType.cpp
      extern/lightsaber/src/utils/Query.cpp
      extern/lightsaber/src/utils/QueryApplication.cpp
      extern/lightsaber/src/utils/Utils.cpp
      extern/lightsaber/src/utils/SystemConf.cpp)

  set(YSB_BENCHMARK_SOURCE_FILES
      src/benchmark/common/rate_limiter.cpp
      src/logging/throughput_logger.cpp
      src/logging/latency_logger.cpp)
endif()

if(${GHOSTWRITER_BUILD_YSB_BENCHMARK})
  # YSB PRODUCER
  add_executable(
    ysb_ghostwriter_producer
    src/benchmark/ysb/ghostwriter/producer.cpp ${COMMON_FILES}
    ${PRODUCER_FILES} ${YSB_BENCHMARK_SOURCE_FILES} ${HEADER_LIST})
  target_link_libraries(ysb_ghostwriter_producer ${UCX_LIBRARIES} flatbuffers
                        ${Boost_LIBRARIES} ${TBB_LIBRARIES} hdr_histogram)
  target_include_directories(ysb_ghostwriter_producer PUBLIC include)
  target_include_directories(
    ysb_ghostwriter_producer PRIVATE ${UCX_INCLUDE_DIRS} ${Boost_INCLUDE_DIRS}
                                     ${TBB_INCLUDE_DIRS})

  # YSB CONSUMER
  add_executable(
    ysb_ghostwriter_consumer
    src/benchmark/ysb/ghostwriter/consumer.cpp ${COMMON_FILES}
    ${CONSUMER_FILES} ${LIGHTSABER_FILES} ${YSB_BENCHMARK_SOURCE_FILES}
    ${HEADER_LIST})
  target_link_libraries(
    ysb_ghostwriter_consumer
    operatorJITLib
    ${TBB_LIBRARIES}
    ${UCX_LIBRARIES}
    ${Boost_LIBRARIES}
    boost_fiber
    boost_fiber_numa
    pthread
    dl
    flatbuffers
    crypto
    hdr_histogram)
  target_include_directories(ysb_ghostwriter_consumer
                             PUBLIC extern/lightsaber/src)
  target_include_directories(ysb_ghostwriter_consumer PUBLIC include)
  target_include_directories(
    ysb_ghostwriter_consumer PRIVATE ${UCX_INCLUDE_DIRS} ${Boost_INCLUDE_DIRS}
                                     ${TBB_INCLUDE_DIRS})
  target_compile_options(ysb_ghostwriter_consumer PRIVATE -Wall -Wextra -O3
                                                          -march=native)
  target_link_options(ysb_ghostwriter_consumer PRIVATE
                      -Wl,--unresolved-symbols=ignore-all)
endif()

if(${GHOSTWRITER_BUILD_YSB_KAFKA_BENCHMARK})
   set(RDKAFKA_BUILD_STATIC ON CACHE BOOL "")

  # YSB PRODUCER
  add_executable(
    ysb_kafka_producer
    src/benchmark/ysb/kafka/producer.cpp ${YSB_BENCHMARK_SOURCE_FILES} ${HEADER_LIST})
  target_link_libraries(ysb_kafka_producer flatbuffers rdkafka++
                        ${Boost_LIBRARIES} ${TBB_LIBRARIES} hdr_histogram)
  target_include_directories(ysb_kafka_producer PUBLIC include)
  target_include_directories(
    ysb_kafka_producer PRIVATE ${Boost_INCLUDE_DIRS}
                                     ${TBB_INCLUDE_DIRS})

  # YSB CONSUMER
  add_executable(
    ysb_kafka_consumer
    src/benchmark/ysb/kafka/consumer.cpp ${LIGHTSABER_FILES} ${YSB_BENCHMARK_SOURCE_FILES}
    ${HEADER_LIST})
  target_link_libraries(
    ysb_kafka_consumer
    operatorJITLib
    ${TBB_LIBRARIES}
    ${Boost_LIBRARIES}
    boost_fiber
    boost_fiber_numa
    pthread
    dl
    flatbuffers
    rdkafka++
    crypto
    hdr_histogram)
  target_include_directories(ysb_kafka_consumer
                             PUBLIC extern/lightsaber/src)
  target_include_directories(ysb_kafka_consumer PUBLIC include)
  target_include_directories(
    ysb_kafka_consumer PRIVATE ${Boost_INCLUDE_DIRS}
                                     ${TBB_INCLUDE_DIRS})
  target_compile_options(ysb_kafka_consumer PRIVATE -Wall -Wextra -O3
                                                          -march=native)
  target_link_options(ysb_kafka_consumer PRIVATE
                      -Wl,--unresolved-symbols=ignore-all)
endif()
if(${GHOSTWRITER_BUILD_YSB_LOCAL_RUNNER})
  # YSB LOCAL RUNNER
  add_executable(
    ysb_local_runner src/benchmark/ysb/local_ysb_runner.cpp ${LIGHTSABER_FILES}
                     ${YSB_BENCHMARK_SOURCE_FILES})
  target_link_options(ysb_local_runner PRIVATE
                      -Wl,--unresolved-symbols=ignore-all)
  target_link_libraries(
    ysb_local_runner
    operatorJITLib
    boost_fiber
    ${TBB_LIBRARIES}
    ${Boost_LIBRARIES}
    TBB::tbb
    boost_fiber_numa
    pthread
    dl)
  target_include_directories(ysb_local_runner PUBLIC extern/lightsaber/src)
  target_include_directories(ysb_local_runner PUBLIC include)
  target_include_directories(ysb_local_runner PRIVATE ${TBB_INCLUDE_DIRS})
  target_compile_options(ysb_local_runner PRIVATE -Wall -Wextra -O3
                                                  -march=native)

  target_link_libraries(ysb_local_runner ${Boost_LIBRARIES} ${TBB_LIBRARIES}
                        crypto hdr_histogram)
  target_include_directories(
    ysb_local_runner PRIVATE ${UCX_INC} ${Boost_INCLUDE_DIRS}
                             ${TBB_INCLUDE_DIRS})
target_include_directories(ysb_local_runner PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${LLVM_INCLUDE_DIRS}
        ${LLVM_BUILD_BINARY_DIR}/tools/clang/include
        ${LLVM_BUILD_MAIN_SRC_DIR}/${CLANG_SRC_DIR_PREFIX}/clang/include
        ${TBB_INCLUDE_DIRS}
        )
# LLVM definitions
separate_arguments(LLVM_DEFINITIONS)
target_compile_definitions(ysb_local_runner PRIVATE
        ${LLVM_DEFINITIONS}
        )


endif()

if(${GHOSTWRITER_BUILD_YSB_DATA_GENERATOR})
  # YSB GENERATOR
  add_executable(lightsaber_data_generator
                 src/benchmark/ysb/YahooBenchmark/lightsaber_data_generator.cpp)
endif()

if(${GHOSTWRITER_BUILD_TESTS})
  # ROUND TRIP TEST
  add_executable(
    round_trip_test
    tests/e2e/round_trip_test.cpp ${COMMON_FILES} ${CONSUMER_FILES}
    ${PRODUCER_FILES} ${MICROBENCHMARKS_SOURCE_FILES} ${HEADER_LIST})
  target_link_libraries(round_trip_test ${UCX_LIBRARIES} flatbuffers
                        ${Boost_LIBRARIES} ${TBB_LIBRARIES} hdr_histogram)
  target_include_directories(round_trip_test PUBLIC include)
  target_include_directories(
    round_trip_test PRIVATE ${UCX_INCLUDE_DIRS} ${Boost_INCLUDE_DIRS}
                            ${TBB_INCLUDE_DIRS})
endif()

if (${GHOSTWRITER_BUILD_NETWORK_TESTS})
  add_executable(
    test_client
    tests/network/client.cpp
    ${COMMON_FILES}
  )
  target_link_libraries(test_client ${UCX_LIBRARIES} flatbuffers)
  target_include_directories(test_client PUBLIC include)
  target_include_directories(test_client PRIVATE ${UCX_INCLUDE_DIRS})

  add_executable(
    test_server
    tests/network/server.cpp
    ${COMMON_FILES}
  )
  target_link_libraries(test_server ${UCX_LIBRARIES} flatbuffers)
  target_include_directories(test_server PUBLIC include)
  target_include_directories(test_server PRIVATE ${UCX_INCLUDE_DIRS})
endif()

#
# add_subdirectory("${PROJECT_SOURCE_DIR}/extern/googletest"
# "extern/googletest")
#
# find_library(UCP_LIB NAMES ucp libucp PATHS $ENV{UCX_BUILD_DIR}/lib
# NO_DEFAULT_PATH) find_library(UCS_LIB NAMES ucs libucs PATHS
# $ENV{UCX_BUILD_DIR}/lib NO_DEFAULT_PATH) find_library(UCT_LIB NAMES uct libuct
# PATHS $ENV{UCX_BUILD_DIR}/lib NO_DEFAULT_PATH) find_path(UCX_INC NAMES
# ucp/api/ucp.h PATHS $ENV{UCX_BUILD_DIR}/include NO_DEFAULT_PATH)
# include_directories(${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR}
# ${UCX_INC})
#
# find_library(UCP_LIB NAMES ucp libucp PATHS $ENV{UCX_BUILD_DIR}/lib
# NO_DEFAULT_PATH) find_package(Boost REQUIRED)
#
# include_directories(${Boost_INCLUDE_DIRS}) macro(package_add_test TESTNAME) #
# create an exectuable in which the tests will be stored
# add_executable(${TESTNAME} ${ARGN}) # link the Google test infrastructure,
# mocking library, and a default main fuction to # the test executable.  Remove
# g_test_main if writing your own main function.
# target_link_libraries(${TESTNAME} gtest gmock gtest_main rembrandtlib
# flatbuffers ${UCP_LIB} ${UCS_LIB} ${UCT_LIB} ${Boost_LIBRARIES}
# ${TBB_LIBRARIES})
#
# target_include_directories(${TESTNAME} PUBLIC include) # gtest_discover_tests
# replaces gtest_add_tests, # see
# https://cmake.org/cmake/help/v3.10/module/GoogleTest.html for more options to
# pass to it gtest_discover_tests(${TESTNAME} # set a working directory so your
# project root so that you can find test data via paths relative to the project
# root WORKING_DIRECTORY ${PROJECT_DIR} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY
# "${PROJECT_DIR}" ) set_target_properties(${TESTNAME} PROPERTIES FOLDER tests)
# endmacro()
#
# package_add_test(BatchTest tests/batch_test.cpp)
# package_add_test(ConnectionManagerTest
# tests/network/connection_manager_test.cpp) package_add_test(DataGeneratorTest
# tests/benchmark/data_generator_test.cpp)
# package_add_test(MessageAccumulatorTest tests/message_accumulator_test.cpp)
# package_add_test(Bla tests/flatbuffer_scratchtest.cpp)
# package_add_test(MessageGeneratorTest
# tests/protocol/message_generator_test.cpp) package_add_test(RateLimiterTest
# tests/benchmark/rate_limiter_test.cpp) package_add_test(SegmentTest
# tests/storage/segment_test.cpp) package_add_test(StorageNodeTest
# tests/storage/storage_node_test.cpp)
