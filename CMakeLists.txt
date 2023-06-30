cmake_minimum_required(VERSION 3.20)
project(
  commandline
  LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_BUILD_TYPE "Debug")

# temp define: https://discourse.llvm.org/t/python-api-problem/945
add_compile_options(-fno-rtti)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  # for debug of stl structure while using clang compile
  add_compile_options(-fstandalone-debug)
endif()

include(CTest)
enable_testing()

# apt install llvm-15-dev libclang-15-dev
set(LLVM_DIR /usr/lib/llvm-15/lib/cmake/llvm)
set(Clang_DIR /usr/lib/llvm-15/lib/cmake/clang)
# manually install
# set(LLVM_DIR /usr/local/lib/cmake/llvm)

find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

include_directories(src)

set(commandline_SRCS)

set(STATIC_LIB_NAME ${PROJECT_NAME})

file(GLOB_RECURSE LIB_PATH
    ./src/*.cc
  )
list(APPEND commandline_SRCS ${LIB_PATH})

add_library(${STATIC_LIB_NAME} STATIC ${commandline_SRCS})
target_link_libraries(${STATIC_LIB_NAME}
  LLVMSupport
  LLVMOption
  LLVMCore
)

add_subdirectory(test)