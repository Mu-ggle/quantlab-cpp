cmake_minimum_required(VERSION 3.22)
project(QuantLab VERSION 1.0.0 DESCRIPTION "Tested C++ time-series analytics" LANGUAGES CXX)

set(CMAKE_CXX_EXTENSIONS OFF)
include(CTest)
include(FetchContent)
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

option(QUANTLAB_BUILD_BENCHMARKS "Build Google Benchmark performance measurements" ON)
option(QUANTLAB_BUILD_DOCS "Build Doxygen HTML documentation" OFF)
option(QUANTLAB_WARNINGS_AS_ERRORS "Treat warnings in project code as errors" ON)
option(QUANTLAB_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

function(quantlab_configure_target target)
    target_compile_features(${target} PUBLIC cxx_std_17)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
        if(QUANTLAB_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
        if(QUANTLAB_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
    if(QUANTLAB_ENABLE_SANITIZERS)
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang" AND NOT WIN32)
            target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
            target_link_options(${target} PRIVATE -fsanitize=address,undefined)
        else()
            message(FATAL_ERROR "QUANTLAB_ENABLE_SANITIZERS requires GCC or Clang on Linux/macOS")
        endif()
    endif()
endfunction()

add_library(quantlab STATIC src/analytics.cpp)
add_library(QuantLab::quantlab ALIAS quantlab)
target_include_directories(quantlab PUBLIC "${PROJECT_SOURCE_DIR}/include")
quantlab_configure_target(quantlab)

add_executable(quantlab_demo src/main.cpp)
target_link_libraries(quantlab_demo PRIVATE QuantLab::quantlab)
quantlab_configure_target(quantlab_demo)

# Exact upstream commits plus SHA-256 hashes make clean builds reproducible.
if(BUILD_TESTING)
    FetchContent_Declare(doctest
        URL https://codeload.github.com/doctest/doctest/tar.gz/1da23a3e8119ec5cce4f9388e91b065e20bf06f5
        URL_HASH SHA256=3ffeeefdd63fa4978a9a835b6007dcbe8d28de45411665285516b3490ca253df)
    set(DOCTEST_WITH_TESTS OFF CACHE BOOL "" FORCE)
    set(DOCTEST_WITH_MAIN_IN_STATIC_LIB OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(doctest)
    add_subdirectory(tests)
endif()

if(QUANTLAB_BUILD_BENCHMARKS)
    FetchContent_Declare(benchmark
        URL https://codeload.github.com/google/benchmark/tar.gz/192ef10025eb2c4cdd392bc502f0c852196baa48
        URL_HASH SHA256=f82705a2726d8f6cdcda274b841f6314dbfc6f731cdda06c946f310ec1cc3ad9)
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_WERROR OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(benchmark)
    add_subdirectory(benchmarks)
endif()

if(QUANTLAB_BUILD_DOCS)
    find_package(Doxygen REQUIRED)
    configure_file(docs/Doxyfile.in "${PROJECT_BINARY_DIR}/Doxyfile" @ONLY)
    add_custom_target(docs
        COMMAND "${DOXYGEN_EXECUTABLE}" "${PROJECT_BINARY_DIR}/Doxyfile"
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
        COMMENT "Generating Doxygen HTML documentation"
        VERBATIM)
endif()
