set(PROJECT_NAME game_server)

################################################################################
# Source groups
################################################################################
set(Header_Files
    "src/comp/gs_scene_comp.h"
    "src/event_handler/event_handler.h"
    "src/event_handler/mission_event_handler.h"
    "src/event_handler/scene_event_handler.h"
    "src/game_server.h"
    "src/handler/common_client_player_handler.h"
    "src/handler/common_server_player_handler.h"
    "src/handler/game_service_handler.h"
    "src/handler/lobby_scene_handler.h"
    "src/handler/player_service.h"
    "src/handler/scene_client_player_handler.h"
    "src/handler/scene_handler.h"
    "src/handler/scene_server_player_handler.h"
    "src/handler/server_service.h"
    "src/handler/team_client_player_handler.h"
    "src/handler/team_handler.h"
    "src/handler/team_server_player_handler.h"
    "src/network/controller_node.h"
    "src/network/gate_node.h"
    "src/network/message_system.h"
    "src/replied_handler/common_client_player_replied_handler.h"
    "src/replied_handler/common_server_player_replied_handler.h"
    "src/replied_handler/controller_service_replied_handler.h"
    "src/replied_handler/deploy_service_replied_handler.h"
    "src/replied_handler/lobby_scene_replied_handler.h"
    "src/replied_handler/player_service_replied.h"
    "src/replied_handler/scene_client_player_replied_handler.h"
    "src/replied_handler/scene_replied_handler.h"
    "src/replied_handler/scene_server_player_replied_handler.h"
    "src/replied_handler/team_client_player_replied_handler.h"
    "src/replied_handler/team_replied_handler.h"
    "src/replied_handler/team_server_player_replied_handler.h"
    "src/system/gs_scene_system.h"
    "src/system/logic/config_system.h"
    "src/system/player_common_system.h"
    "src/system/player_scene_system.h"
    "src/system/player_tip_system.h"
    "src/system/recast_system.h"
    "src/system/redis_system.h"
    "src/thread_local/game_thread_local_storage.h"
)
source_group("Header Files" FILES ${Header_Files})

set(Source_Files
    "main.cpp"
    "src/event_handler/event_handler.cpp"
    "src/event_handler/mission_event_handler.cpp"
    "src/event_handler/scene_event_handler.cpp"
    "src/game_server.cpp"
    "src/handler/common_client_player_handler.cpp"
    "src/handler/common_server_player_handler.cpp"
    "src/handler/game_service_handler.cpp"
    "src/handler/lobby_scene_handler.cpp"
    "src/handler/player_service.cpp"
    "src/handler/scene_client_player_handler.cpp"
    "src/handler/scene_handler.cpp"
    "src/handler/scene_server_player_handler.cpp"
    "src/handler/server_service.cpp"
    "src/handler/team_client_player_handler.cpp"
    "src/handler/team_handler.cpp"
    "src/handler/team_server_player_handler.cpp"
    "src/network/controller_node.cpp"
    "src/network/gate_node.cpp"
    "src/network/message_system.cpp"
    "src/replied_handler/common_client_player_replied_handler.cpp"
    "src/replied_handler/common_server_player_replied_handler.cpp"
    "src/replied_handler/controller_service_replied_handler.cpp"
    "src/replied_handler/deploy_service_replied_handler.cpp"
    "src/replied_handler/lobby_scene_replied_handler.cpp"
    "src/replied_handler/player_service_replied.cpp"
    "src/replied_handler/register_replied_handler.cpp"
    "src/replied_handler/scene_client_player_replied_handler.cpp"
    "src/replied_handler/scene_replied_handler.cpp"
    "src/replied_handler/scene_server_player_replied_handler.cpp"
    "src/replied_handler/team_client_player_replied_handler.cpp"
    "src/replied_handler/team_replied_handler.cpp"
    "src/replied_handler/team_server_player_replied_handler.cpp"
    "src/system/gs_scene_system.cpp"
    "src/system/logic/config_system.cpp"
    "src/system/player_common_system.cpp"
    "src/system/player_scene_system.cpp"
    "src/system/player_tip_system.cpp"
    "src/system/recast_system.cpp"
    "src/system/redis_system.cpp"
    "src/thread_local/game_thread_local_storage.cpp"
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
set(ROOT_NAMESPACE game_server)

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
elseif("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x86")
    set_target_properties(${PROJECT_NAME} PROPERTIES
        OUTPUT_DIRECTORY_DEBUG   "${CMAKE_CURRENT_SOURCE_DIR}/../bin/"
    )
endif()
################################################################################
# MSVC runtime library
################################################################################
get_property(MSVC_RUNTIME_LIBRARY_DEFAULT TARGET ${PROJECT_NAME} PROPERTY MSVC_RUNTIME_LIBRARY)
if("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x86")
    string(CONCAT "MSVC_RUNTIME_LIBRARY_STR"
        $<$<CONFIG:Debug>:
            MultiThreadedDebugDLL
        >
        $<$<CONFIG:Release>:
            MultiThreadedDLL
        >
        $<$<NOT:$<OR:$<CONFIG:Debug>,$<CONFIG:Release>>>:${MSVC_RUNTIME_LIBRARY_DEFAULT}>
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
            "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/ue5navmesh/Public;"
            "${CMAKE_CURRENT_SOURCE_DIR}/../common/src/pb/pbc/common_proto"
        ">"
    )
elseif("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x86")
    target_include_directories(${PROJECT_NAME} PUBLIC
        "$<$<CONFIG:Debug>:"
            "${CMAKE_CURRENT_SOURCE_DIR}/../third_party;"
            "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/boost;"
            "${CMAKE_CURRENT_SOURCE_DIR}/.;"
            "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/protobuf/src;"
            "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/zlib;"
            "${CMAKE_CURRENT_SOURCE_DIR}/../common"
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
            "ENTT_ID_TYPE=uint64_t;"
            "_TCHAR_DEFINED;"
            "UNICODE"
        ">"
    )
elseif("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x86")
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        "$<$<CONFIG:Release>:"
            "NDEBUG;"
            "_CONSOLE"
        ">"
        "WIN32;"
        "_CRT_SECURE_NO_WARNINGS;"
        "_WINSOCK_DEPRECATED_NO_WARNINGS;"
        "_CRT_SECURE_NO_DEPRECATE;"
        "_CRT_NONSTDC_NO_DEPRECATE"
    )
endif()

################################################################################
# Compile and link options
################################################################################
if(MSVC)
    if("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
        target_compile_options(${PROJECT_NAME} PRIVATE
            $<$<CONFIG:Debug>:
                /MP;
                /std:c++17;
                /WX;
                /wd4200;
                /wd4819;
                /utf-8
            >
            ${DEFAULT_CXX_DEBUG_INFORMATION_FORMAT};
            ${DEFAULT_CXX_EXCEPTION_HANDLING}
        )
    elseif("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x86")
        target_compile_options(${PROJECT_NAME} PRIVATE
            $<$<CONFIG:Debug>:
                /Od
            >
            /W3;
            /Zi;
            ${DEFAULT_CXX_EXCEPTION_HANDLING}
        )
    endif()
    if("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
        target_link_options(${PROJECT_NAME} PRIVATE
            $<$<CONFIG:Debug>:
                /MANIFEST:NO
            >
            /DEBUG:FULL
        )
    elseif("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x86")
        target_link_options(${PROJECT_NAME} PRIVATE
            $<$<CONFIG:Release>:
                /OPT:REF;
                /OPT:ICF
            >
            /DEBUG;
            /MACHINE:X86;
            /SUBSYSTEM:CONSOLE;
            /INCREMENTAL
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
elseif("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x86")
    set(ADDITIONAL_LIBRARY_DEPENDENCIES
        "$<$<CONFIG:Debug>:"
            "third_party;"
            "common;"
            "libprotobufd;"
            "zlib"
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
elseif("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x86")
    target_link_directories(${PROJECT_NAME} PRIVATE
        "$<$<CONFIG:Debug>:"
            "${CMAKE_CURRENT_SOURCE_DIR}/../bin/"
        ">"
    )
endif()

