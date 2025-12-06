cmake_minimum_required(VERSION 3.20)
project(NoMemberPointerPlugin LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)  # SHARED 库自动导出符号

# 修改为你实际 LLVM 解压路径
set(LLVM_ROOT "D:/game/llvm")
set(LLVM_DIR  "${LLVM_ROOT}/lib/cmake/llvm")
set(Clang_DIR "${LLVM_ROOT}/lib/cmake/clang")

# 查找 LLVM 和 Clang
find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)

# 包含头文件
include_directories(
    ${LLVM_INCLUDE_DIRS}
    ${CLANG_INCLUDE_DIRS}
)

# 生成 DLL（clang-tidy 插件）
add_library(NoMemberPointerPlugin SHARED
    NoMemberRawPointerCheck.cpp
    NoMemberRawPointerCheck.h
)

# 链接 clang-tidy 所需库，并解决 __delayLoadHelper2 问题
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
    delayimp           # <-- MSVC 延迟加载支持
)

# 如果 VS 环境提示找不到 delayimp.lib，可以显式指定路径
# target_link_directories(NoMemberPointerPlugin PRIVATE
#     "C:/Program Files/Microsoft Visual Studio/2026/VC/Tools/MSVC/<version>/lib/x64"
# )

# 设置编译选项（可选，确保 DLL 不生成 CRT 错误）
if(MSVC)
    target_compile_options(NoMemberPointerPlugin PRIVATE /bigobj)
endif()
