cmake_minimum_required(VERSION 3.25)
project(memsim VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)

option(MEMSIM_WERROR "Treat warnings as errors" ON)
option(MEMSIM_SANITIZE "Build with AddressSanitizer and UBSan" OFF)

# Warnings and sanitizers for first-party code; third_party/ does not link this.
add_library(memsim_flags INTERFACE)
target_compile_options(memsim_flags INTERFACE
  -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
  -Wstrict-prototypes -Wmissing-prototypes -Wvla
  $<$<BOOL:${MEMSIM_WERROR}>:-Werror>)
if(MEMSIM_SANITIZE)
  target_compile_options(memsim_flags INTERFACE -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(memsim_flags INTERFACE -fsanitize=address,undefined)
endif()

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)
include(MemsimModule)

add_subdirectory(third_party)
add_subdirectory(src)

include(CTest)
if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
