# 保留现有 Windows 工程的库名；仅改本次构建目标，不修改第三方源码。
# 上游静态库设置了 ARCHIVE_OUTPUT_NAME，必须一并覆盖，单改 OUTPUT_NAME 不生效。
if(PROJECT_NAME STREQUAL "zlib" AND MSVC)
    cmake_language(DEFER CALL set_target_properties zlib PROPERTIES
        OUTPUT_NAME zlib ARCHIVE_OUTPUT_NAME zlib)
    cmake_language(DEFER CALL set_target_properties zlibstatic PROPERTIES
        OUTPUT_NAME zlibstatic ARCHIVE_OUTPUT_NAME zlibstatic)
endif()
