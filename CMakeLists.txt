cmake_minimum_required(VERSION 4.0)

project(embedrite CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# The in-process unwinder (embdr/cxxstd/unwind.cppm) starts its walk from the
# caller frame of unwind_capture_current via __builtin_frame_address(1), and
# its frame-pointer fallback walks [fp]/[fp+8] records. Keep frame pointers
# enabled in every build type so both stay valid under optimization.
if (MSVC)
  add_compile_options(/Oy-)
else ()
  add_compile_options(-fno-omit-frame-pointer)
endif ()

set(CXXSTD_SOURCES
  embdr/cxxstd/memoryIterator.cppm
  embdr/cxxstd/memoryAllocator.cppm
  embdr/cxxstd/unwind.cppm
  embdr/cxxstd/unwindPrettify.cppm
  embdr/cxxstd/sigHandler.cppm
  embdr/cxxstd/memoryMaybe.cppm
  embdr/cxxstd/stringFormat.cppm
  embdr/cxxstd/stringifyFloat.cppm
  embdr/cxxstd/stringifyNumber.cppm
  embdr/cxxstd/stringView.cppm
  embdr/cxxstd/io.cppm
  embdr/cxxstd/timeCore.cppm
  embdr/cxxstd/timeUtil.cppm
  embdr/cxxstd/consoleUtil.cppm
  embdr/cxxstd/logger.cppm
)
set(CXXSTD_MODULES
  embdr.cxxstd.memoryIterator
  embdr.cxxstd.memoryAllocator
  embdr.cxxstd.memoryMaybe
  embdr.cxxstd.unwind
  embdr.cxxstd.unwindPrettify
  embdr.cxxstd.sigHandler
  embdr.cxxstd.stringView
  embdr.cxxstd.stringFormat
  embdr.cxxstd.stringifyFloat
  embdr.cxxstd.stringNumber
  embdr.cxxstd.io
  embdr.cxxstd.timeCore
  embdr.cxxstd.timeUtil
  embdr.cxxstd.consoleUtil
  embdr.cxxstd.logger
)
add_library(embdc-compiler STATIC)
target_sources(
  embdc-compiler
  PUBLIC
  FILE_SET CXX_MODULES TYPE CXX_MODULES FILES
  ${CXXSTD_SOURCES}
)

target_include_directories(embdc-compiler PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
add_executable(embdc embdr/embdc/main.cpp)
target_link_libraries(embdc PRIVATE embdc-compiler)

if (NOT EMBDR_DISABLE_ASAN)
  if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_definitions(EMBDR_DEV)
    if (MSVC)
      add_link_options(/INCREMENTAL:NO)
      add_compile_options(/EHs-c-)
      add_compile_options(/GR-)
      add_compile_options(/Gy)
      add_link_options(/OPT:REF)
    else ()
      add_link_options(-fsanitize=address)
    endif ()
  endif ()
endif ()

if (CMAKE_BUILD_TYPE STREQUAL "Release")
  if (MSVC)
    target_compile_options(embdc-compiler INTERFACE /EHs-c- /GR-)
  endif()
  if (NOT MSVC)
    # The project is not embedded, But is nice to remove binary bloat
    add_link_options(-fno-exceptions)
    add_link_options(-fno-rtti)
    add_link_options(-ffunction-sections)
    add_link_options(-fdata-sections)
    add_link_options(-fno-use-cxa-atexit)
    add_link_options(-fno-unwind-tables)
    add_link_options(-fno-asynchronous-unwind-tables)
    add_link_options(-Wl,--gc-sections)
  endif ()
endif ()

option(EMBDR_BUILD_TESTS "Build embdr unit tests" ON)
if (EMBDR_BUILD_TESTS)
  enable_testing()
  add_subdirectory(third-party/catch2)

  add_executable(embdr-tests
    embdr/tests/unwind_test.cpp
    embdr/tests/unwindPrettify_test.cpp
    embdr/tests/sigHandler_test.cpp
    embdr/tests/io_test.cpp
    embdr/tests/timeCore_test.cpp
    embdr/tests/timeUtil_test.cpp
    embdr/tests/consoleUtil_test.cpp
    embdr/tests/logger_test.cpp
    embdr/tests/stringFormat_test.cpp
  )
  target_link_libraries(embdr-tests PRIVATE embdc-compiler Catch2::Catch2WithMain)

  include(${CMAKE_CURRENT_SOURCE_DIR}/third-party/catch2/extras/Catch.cmake)
  catch_discover_tests(embdr-tests)
endif ()

include(CheckIPOSupported)
check_ipo_supported(RESULT IPO_SUPPORTED OUTPUT IPO_OUTPUT)
if (IPO_SUPPORTED)
  set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
endif ()