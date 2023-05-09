set(PROJECT_NAME lobby_server)

################################################################################
# Source groups
################################################################################
set(Header_Files
    "src/lobby_server.h"
    "src/network/controller_node.h"
    "src/network/gs_node.h"
    "src/service/deploy_service_replied.h"
    "src/service/lobby_scene.h"
    "src/service/replied_dispathcer.h"
    "src/thread_local/lobby_thread_local_storage.h"
)
source_group("Header Files" FILES ${Header_Files})

set(Source_Files
    "main.cpp"
    "src/lobby_server.cpp"
    "src/network/controller_node.cpp"
    "src/network/gs_node.cpp"
    "src/service/deploy_service_replied.cpp"
    "src/service/lobby_scene.cpp"
    "src/service/replied_dispathcer.cpp"
    "src/thread_local/lobby_thread_local_storage.cpp"
)
source_group("Source Files" FILES ${Source_Files})

set(ALL_FILES
    ${Header_Files}
    ${Source_Files}
)

################################################################################
# Target
################################################################################
add_executable(${PROJECT_NAME} ${ALL_FILES})

use_props(${PROJECT_NAME} "${CMAKE_CONFIGURATION_TYPES}" "${DEFAULT_CXX_PROPS}")
set(ROOT_NAMESPACE lobby_server)

set_target_properties(${PROJECT_NAME} PROPERTIES
    VS_GLOBAL_KEYWORD "Win32Proj"
)
################################################################################
# Output directory
################################################################################
if("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
    set_target_properties(${PROJECT_NAME} PROPERTIES
        OUTPUT_DIRECTORY_DEBUG   "${CMAKE_CURRENT_SOURCE_DIR}/../bin/"
    )
endif()
if("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
    set_target_properties(${PROJECT_NAME} PROPERTIES
        INTERPROCEDURAL_OPTIMIZATION_RELEASE "TRUE"
    )
elseif("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x86")
    set_target_properties(${PROJECT_NAME} PROPERTIES
        INTERPROCEDURAL_OPTIMIZATION_RELEASE "TRUE"
    )
endif()
################################################################################
# Include directories
################################################################################
if("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
    target_include_directories(${PROJECT_NAME} PUBLIC
        "$<$<CONFIG:Debug>:"
            "${CMAKE_CURRENT_SOURCE_DIR}/../third_party;"
            "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/boost;"
            "${CMAKE_CURRENT_SOURCE_DIR}/.;"
            "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/protobuf/src;"
            "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/zlib;"
            "${CMAKE_CURRENT_SOURCE_DIR}/../common;"
            "${CMAKE_CURRENT_SOURCE_DIR}/../common/src/pb/pbc;"
            "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/redis;"
            "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/muduo;"
            "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/muduo/contrib/windows/include;"
            "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/redis/deps;"
            "${CMAKE_CURRENT_SOURCE_DIR}/../common/src/pb/pbc/common_proto"
        ">"
    )
endif()

################################################################################
# Compile definitions
################################################################################
if("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        "$<$<CONFIG:Debug>:"
            "WIN32;"
            "_CRT_SECURE_NO_WARNINGS;"
            "_WINSOCK_DEPRECATED_NO_WARNINGS;"
            "_CRT_SECURE_NO_DEPRECATE;"
            "_CRT_NONSTDC_NO_DEPRECATE;"
            "ENTT_ID_TYPE=uint64_t"
        ">"
        "$<$<CONFIG:Release>:"
            "NDEBUG;"
            "_CONSOLE"
        ">"
        "UNICODE;"
        "_UNICODE"
    )
elseif("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x86")
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        "$<$<CONFIG:Debug>:"
            "_DEBUG"
        ">"
        "$<$<CONFIG:Release>:"
            "NDEBUG"
        ">"
        "WIN32;"
        "_CONSOLE;"
        "UNICODE;"
        "_UNICODE"
    )
endif()

################################################################################
# Compile and link options
################################################################################
if(MSVC)
    if("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
        target_compile_options(${PROJECT_NAME} PRIVATE
            $<$<CONFIG:Debug>:
                /std:c++17;
                /wd4819;
                /utf-8
            >
            $<$<CONFIG:Release>:
                /Oi;
                /Gy
            >
            /permissive-;
            /sdl;
            /W3;
            ${DEFAULT_CXX_DEBUG_INFORMATION_FORMAT};
            ${DEFAULT_CXX_EXCEPTION_HANDLING}
        )
    elseif("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x86")
        target_compile_options(${PROJECT_NAME} PRIVATE
            $<$<CONFIG:Release>:
                /Oi;
                /Gy
            >
            /permissive-;
            /sdl;
            /W3;
            ${DEFAULT_CXX_DEBUG_INFORMATION_FORMAT};
            ${DEFAULT_CXX_EXCEPTION_HANDLING}
        )
    endif()
    if("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
        target_link_options(${PROJECT_NAME} PRIVATE
            $<$<CONFIG:Debug>:
                /INCREMENTAL
            >
            $<$<CONFIG:Release>:
                /OPT:REF;
                /OPT:ICF;
                /INCREMENTAL:NO
            >
            /DEBUG;
            /SUBSYSTEM:CONSOLE
        )
    elseif("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x86")
        target_link_options(${PROJECT_NAME} PRIVATE
            $<$<CONFIG:Debug>:
                /INCREMENTAL
            >
            $<$<CONFIG:Release>:
                /OPT:REF;
                /OPT:ICF;
                /INCREMENTAL:NO
            >
            /DEBUG;
            /SUBSYSTEM:CONSOLE
        )
    endif()
endif()

################################################################################
# Dependencies
################################################################################
add_dependencies(${PROJECT_NAME}
    common
    third_party
)

if("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
    set(ADDITIONAL_LIBRARY_DEPENDENCIES
        "$<$<CONFIG:Debug>:"
            "third_party;"
            "common;"
            "libprotobufd;"
            "zlib;"
            "hiredis"
        ">"
    )
endif()
target_link_libraries(${PROJECT_NAME} PRIVATE "${ADDITIONAL_LIBRARY_DEPENDENCIES}")

if("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
    target_link_directories(${PROJECT_NAME} PRIVATE
        "$<$<CONFIG:Debug>:"
            "${CMAKE_CURRENT_SOURCE_DIR}/../bin/"
        ">"
    )
endif()

