cmake_minimum_required(VERSION 3.20)
project(NoMemberPointerPlugin LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

# 修改这个路径为你实际解压路径
set(LLVM_ROOT "D:/game/llvm")

set(LLVM_DIR  "${LLVM_ROOT}/lib/cmake/llvm")
set(Clang_DIR "${LLVM_ROOT}/lib/cmake/clang")

find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)

include_directories(
    ${LLVM_INCLUDE_DIRS}
    ${CLANG_INCLUDE_DIRS}
)

add_library(NoMemberPointerPlugin SHARED
    NoMemberRawPointerCheck.cpp
    NoMemberRawPointerCheck.h
)

target_link_libraries(NoMemberPointerPlugin PRIVATE
    clangTidy
    clangTidyPlugin
    clangAST
    clangASTMatchers
    clangBasic
    clangFrontend
    clangTooling
    clangRewrite
    clangLex
)
