cmake_minimum_required(VERSION 3.30)
project(MacroNoMore)

set(LLVM_LINK_COMPONENTS
        Support
)

find_package(LLVM REQUIRED CONFIG HINTS ${LLVM_DIR})
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(AddLLVM)

find_package(Clang REQUIRED)

message("LLVM STATUS:
  Definitions ${LLVM_DEFINITIONS}
  Includes    ${LLVM_INCLUDE_DIRS}
${CLANG_INCLUDE_DIRS}
  Libraries   ${LLVM_LIBRARY_DIRS}"
)

include_directories(SYSTEM ${LLVM_INCLUDE_DIRS} ${CLANG_INCLUDE_DIRS})
link_directories(${LLVM_LIBRARY_DIRS})
add_definitions(${LLVM_DEFINITIONS})

include_directories(${CMAKE_SOURCE_DIR}/include)
link_directories(${LIBRARY_OUTPUT_PATH})
set(CMAKE_TEMP_LIBRARY_PATH "${PROJECT_BINARY_DIR}/lib")

include("/usr/lib/llvm-14/lib/cmake/clang/AddClang.cmake")

list(FILTER SOURCE_FILES EXCLUDE REGEX "${CMAKE_SOURCE_DIR}/test/.*")

add_clang_executable(MacroNoMore MacroNoMore.cpp)

target_link_libraries(MacroNoMore
        PRIVATE
        clangAST
        clangBasic
        clangFrontend
        clangSerialization
        clangTooling
)

# Cross-compilation settings for Windows
if (WIN32)
    set(CMAKE_SYSTEM_NAME Windows)
    set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
    set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
    set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)
    set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)
    set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
    set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
    set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
endif()

# Ensure necessary headers are included
include(CheckIncludeFile)
check_include_file(sys/types.h HAVE_SYS_TYPES_H)
check_include_file(mutex HAVE_MUTEX)

if (HAVE_SYS_TYPES_H)
    add_definitions(-DHAVE_SYS_TYPES_H)
endif()

if (HAVE_MUTEX)
    add_definitions(-DHAVE_MUTEX)
endif()