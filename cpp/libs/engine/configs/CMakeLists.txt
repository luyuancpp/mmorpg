set(PROJECT_NAME common)

################################################################################
# Source groups
################################################################################
set(no_group_source_files
    "src/common_type/common_type.h"
    "src/event/event.cc"
    "src/event/event.h"
    "src/game_config/all_config.cpp"
    "src/game_config/all_config.h"
    "src/game_config/condition_config.cpp"
    "src/game_config/condition_config.h"
    "src/game_config/deploy_json.cpp"
    "src/game_config/deploy_json.h"
    "src/game_config/global_variable_config.cpp"
    "src/game_config/global_variable_config.h"
    "src/game_config/global_variable_config_id.h"
    "src/game_config/item_config.cpp"
    "src/game_config/item_config.h"
    "src/game_config/lobby_config.cpp"
    "src/game_config/lobby_config.h"
    "src/game_config/mainscene_config.cpp"
    "src/game_config/mainscene_config.h"
    "src/game_config/mission_config.cpp"
    "src/game_config/mission_config.h"
    "src/game_config/monster_base_config.cpp"
    "src/game_config/monster_base_config.h"
    "src/game_config/scene_config.cpp"
    "src/game_config/scene_config.h"
    "src/game_logic/bag/bag.cpp"
    "src/game_logic/bag/bag.h"
    "src/game_logic/bag/item.cpp"
    "src/game_logic/bag/item.h"
    "src/game_logic/comp/account_comp.h"
    "src/game_logic/comp/player_comp.h"
    "src/game_logic/comp/scene_comp.h"
    "src/game_logic/config_expression/config_expression.h"
    "src/game_logic/constants/time_variable.h"
    "src/game_logic/exprtk/exprtk.hpp"
    "src/game_logic/login_state/login_state.cpp"
    "src/game_logic/login_state/login_state.h"
    "src/game_logic/login_state/login_state_concrete.h"
    "src/game_logic/login_state/login_state_machine.cpp"
    "src/game_logic/login_state/login_state_machine.h"
    "src/game_logic/missions/missions_base.cpp"
    "src/game_logic/missions/missions_base.h"
    "src/game_logic/missions/missions_config_template.h"
    "src/game_logic/player/player_list.cpp"
    "src/game_logic/player/player_list.h"
    "src/game_logic/scene/scene.cpp"
    "src/game_logic/scene/scene.h"
    "src/game_logic/scene/servernode_system.cpp"
    "src/game_logic/scene/servernode_system.h"
    "src/game_logic/teams/team.cpp"
    "src/game_logic/teams/team.h"
    "src/game_logic/teams/team_event.h"
    "src/game_logic/thread_local/common_logic_thread_local_storage.cpp"
    "src/game_logic/thread_local/common_logic_thread_local_storage.h"
    "src/game_logic/thread_local/thread_local_storage.cpp"
    "src/game_logic/thread_local/thread_local_storage.h"
    "src/game_logic/thread_local/thread_local_storage_lua.cpp"
    "src/game_logic/thread_local/thread_local_storage_lua.h"
    "src/game_logic/timer_task/timer_task.cpp"
    "src/game_logic/timer_task/timer_task.h"
    "src/game_logic/tips_id.h"
    "src/jsoncpp/json.h"
    "src/jsoncpp/jsoncpp.cpp"
    "src/mysql_wrapper/message_2_mysql.cpp"
    "src/mysql_wrapper/message_2_mysql.h"
    "src/mysql_wrapper/mysql_client.cpp"
    "src/mysql_wrapper/mysql_client.h"
    "src/mysql_wrapper/mysql_database.cpp"
    "src/mysql_wrapper/mysql_database.h"
    "src/mysql_wrapper/mysql_result.h"
    "src/network/codec/codec.cc"
    "src/network/codec/codec.h"
    "src/network/codec/dispatcher.h"
    "src/network/codec/dispatcher_lite.h"
    "src/network/game_rpc.pb.cc"
    "src/network/game_rpc.pb.h"
    "src/network/gate_node.h"
    "src/network/node_info.h"
    "src/network/rpc_channel.cc"
    "src/network/rpc_channel.h"
    "src/network/rpc_client.h"
    "src/network/rpc_connection_event.h"
    "src/network/rpc_msg_route.cpp"
    "src/network/rpc_msg_route.h"
    "src/network/rpc_server.cc"
    "src/network/rpc_server.h"
    "src/network/server_component.h"
    "src/network/session.cpp"
    "src/network/session.h"
    "src/pb/pbc/client_player_proto/common_client_player.pb.cc"
    "src/pb/pbc/client_player_proto/common_client_player.pb.h"
    "src/pb/pbc/client_player_proto/scene_client_player.pb.cc"
    "src/pb/pbc/client_player_proto/scene_client_player.pb.h"
    "src/pb/pbc/client_player_proto/team_client_player.pb.cc"
    "src/pb/pbc/client_player_proto/team_client_player.pb.h"
    "src/pb/pbc/common_client_player_service.cpp"
    "src/pb/pbc/common_client_player_service.h"
    "src/pb/pbc/common_proto/account_database.pb.cc"
    "src/pb/pbc/common_proto/account_database.pb.h"
    "src/pb/pbc/common_proto/c2gate.pb.cc"
    "src/pb/pbc/common_proto/c2gate.pb.h"
    "src/pb/pbc/common_proto/common.pb.cc"
    "src/pb/pbc/common_proto/common.pb.h"
    "src/pb/pbc/common_proto/common_message.pb.cc"
    "src/pb/pbc/common_proto/common_message.pb.h"
    "src/pb/pbc/common_proto/comp.pb.cc"
    "src/pb/pbc/common_proto/comp.pb.h"
    "src/pb/pbc/common_proto/controller_service.pb.cc"
    "src/pb/pbc/common_proto/controller_service.pb.h"
    "src/pb/pbc/common_proto/database_service.pb.cc"
    "src/pb/pbc/common_proto/database_service.pb.h"
    "src/pb/pbc/common_proto/db_base.pb.cc"
    "src/pb/pbc/common_proto/db_base.pb.h"
    "src/pb/pbc/common_proto/deploy_service.pb.cc"
    "src/pb/pbc/common_proto/deploy_service.pb.h"
    "src/pb/pbc/common_proto/game_service.pb.cc"
    "src/pb/pbc/common_proto/game_service.pb.h"
    "src/pb/pbc/common_proto/gate_service.pb.cc"
    "src/pb/pbc/common_proto/gate_service.pb.h"
    "src/pb/pbc/common_proto/login_service.pb.cc"
    "src/pb/pbc/common_proto/login_service.pb.h"
    "src/pb/pbc/common_proto/mysql_database_table.pb.cc"
    "src/pb/pbc/common_proto/mysql_database_table.pb.h"
    "src/pb/pbc/common_proto/mysql_database_test.pb.cc"
    "src/pb/pbc/common_proto/mysql_database_test.pb.h"
    "src/pb/pbc/common_proto/node_common.pb.cc"
    "src/pb/pbc/common_proto/node_common.pb.h"
    "src/pb/pbc/component_proto/gs_node_comp.pb.cc"
    "src/pb/pbc/component_proto/gs_node_comp.pb.h"
    "src/pb/pbc/component_proto/item_base_comp.pb.cc"
    "src/pb/pbc/component_proto/item_base_comp.pb.h"
    "src/pb/pbc/component_proto/item_comp.pb.cc"
    "src/pb/pbc/component_proto/item_comp.pb.h"
    "src/pb/pbc/component_proto/mission_comp.pb.cc"
    "src/pb/pbc/component_proto/mission_comp.pb.h"
    "src/pb/pbc/component_proto/node_comp.pb.cc"
    "src/pb/pbc/component_proto/node_comp.pb.h"
    "src/pb/pbc/component_proto/player_async_comp.pb.cc"
    "src/pb/pbc/component_proto/player_async_comp.pb.h"
    "src/pb/pbc/component_proto/player_comp.pb.cc"
    "src/pb/pbc/component_proto/player_comp.pb.h"
    "src/pb/pbc/component_proto/player_login_comp.pb.cc"
    "src/pb/pbc/component_proto/player_login_comp.pb.h"
    "src/pb/pbc/component_proto/player_network_comp.pb.cc"
    "src/pb/pbc/component_proto/player_network_comp.pb.h"
    "src/pb/pbc/component_proto/scene_comp.pb.cc"
    "src/pb/pbc/component_proto/scene_comp.pb.h"
    "src/pb/pbc/component_proto/team_comp.pb.cc"
    "src/pb/pbc/component_proto/team_comp.pb.h"
    "src/pb/pbc/controller_common_server_player_service.cpp"
    "src/pb/pbc/controller_common_server_player_service.h"
    "src/pb/pbc/controller_scene_server_player_service.cpp"
    "src/pb/pbc/controller_scene_server_player_service.h"
    "src/pb/pbc/controller_scene_service.cpp"
    "src/pb/pbc/controller_scene_service.h"
    "src/pb/pbc/controller_service_service.cpp"
    "src/pb/pbc/controller_service_service.h"
    "src/pb/pbc/database_service_service.cpp"
    "src/pb/pbc/database_service_service.h"
    "src/pb/pbc/deploy_service_service.cpp"
    "src/pb/pbc/deploy_service_service.h"
    "src/pb/pbc/event_proto/mission_event.pb.cc"
    "src/pb/pbc/event_proto/mission_event.pb.h"
    "src/pb/pbc/event_proto/scene_event.pb.cc"
    "src/pb/pbc/event_proto/scene_event.pb.h"
    "src/pb/pbc/game_config/condition_config.pb.cc"
    "src/pb/pbc/game_config/condition_config.pb.h"
    "src/pb/pbc/game_config/global_variable_config.pb.cc"
    "src/pb/pbc/game_config/global_variable_config.pb.h"
    "src/pb/pbc/game_config/item_config.pb.cc"
    "src/pb/pbc/game_config/item_config.pb.h"
    "src/pb/pbc/game_config/mainscene_config.pb.cc"
    "src/pb/pbc/game_config/mainscene_config.pb.h"
    "src/pb/pbc/game_config/mission_config.pb.cc"
    "src/pb/pbc/game_config/mission_config.pb.h"
    "src/pb/pbc/game_config/monster_base_config.pb.cc"
    "src/pb/pbc/game_config/monster_base_config.pb.h"
    "src/pb/pbc/game_config/scene_config.pb.cc"
    "src/pb/pbc/game_config/scene_config.pb.h"
    "src/pb/pbc/game_scene_server_player_service.cpp"
    "src/pb/pbc/game_scene_server_player_service.h"
    "src/pb/pbc/game_scene_service.cpp"
    "src/pb/pbc/game_scene_service.h"
    "src/pb/pbc/game_server_player_service.cpp"
    "src/pb/pbc/game_server_player_service.h"
    "src/pb/pbc/game_service_service.cpp"
    "src/pb/pbc/game_service_service.h"
    "src/pb/pbc/gate_service_service.cpp"
    "src/pb/pbc/gate_service_service.h"
    "src/pb/pbc/lobby_scene_service.cpp"
    "src/pb/pbc/lobby_scene_service.h"
    "src/pb/pbc/logic_proto/controller_scene.pb.cc"
    "src/pb/pbc/logic_proto/controller_scene.pb.h"
    "src/pb/pbc/logic_proto/controller_team.pb.cc"
    "src/pb/pbc/logic_proto/controller_team.pb.h"
    "src/pb/pbc/logic_proto/game_scene.pb.cc"
    "src/pb/pbc/logic_proto/game_scene.pb.h"
    "src/pb/pbc/logic_proto/game_team.pb.cc"
    "src/pb/pbc/logic_proto/game_team.pb.h"
    "src/pb/pbc/logic_proto/lobby_scene.pb.cc"
    "src/pb/pbc/logic_proto/lobby_scene.pb.h"
    "src/pb/pbc/login_service_service.cpp"
    "src/pb/pbc/login_service_service.h"
    "src/pb/pbc/lua/account_database_sol2.cpp"
    "src/pb/pbc/lua/c2gate_sol2.cpp"
    "src/pb/pbc/lua/common_client_player_service_lua.cpp"
    "src/pb/pbc/lua/common_client_player_sol2.cpp"
    "src/pb/pbc/lua/common_message_sol2.cpp"
    "src/pb/pbc/lua/common_sol2.cpp"
    "src/pb/pbc/lua/comp_sol2.cpp"
    "src/pb/pbc/lua/controller_common_server_player_service_lua.cpp"
    "src/pb/pbc/lua/controller_common_server_player_sol2.cpp"
    "src/pb/pbc/lua/controller_scene_server_player_service_lua.cpp"
    "src/pb/pbc/lua/controller_scene_server_player_sol2.cpp"
    "src/pb/pbc/lua/controller_scene_service_lua.cpp"
    "src/pb/pbc/lua/controller_scene_sol2.cpp"
    "src/pb/pbc/lua/controller_service_service_lua.cpp"
    "src/pb/pbc/lua/controller_service_sol2.cpp"
    "src/pb/pbc/lua/controller_team_sol2.cpp"
    "src/pb/pbc/lua/database_service_service_lua.cpp"
    "src/pb/pbc/lua/database_service_sol2.cpp"
    "src/pb/pbc/lua/db_base_sol2.cpp"
    "src/pb/pbc/lua/deploy_service_service_lua.cpp"
    "src/pb/pbc/lua/deploy_service_sol2.cpp"
    "src/pb/pbc/lua/game_scene_server_player_service_lua.cpp"
    "src/pb/pbc/lua/game_scene_server_player_sol2.cpp"
    "src/pb/pbc/lua/game_scene_service_lua.cpp"
    "src/pb/pbc/lua/game_scene_sol2.cpp"
    "src/pb/pbc/lua/game_server_player_service_lua.cpp"
    "src/pb/pbc/lua/game_server_player_sol2.cpp"
    "src/pb/pbc/lua/game_service_service_lua.cpp"
    "src/pb/pbc/lua/game_service_sol2.cpp"
    "src/pb/pbc/lua/game_team_sol2.cpp"
    "src/pb/pbc/lua/gate_service_service_lua.cpp"
    "src/pb/pbc/lua/gate_service_sol2.cpp"
    "src/pb/pbc/lua/gs_node_comp_sol2.cpp"
    "src/pb/pbc/lua/item_base_comp_sol2.cpp"
    "src/pb/pbc/lua/item_comp_sol2.cpp"
    "src/pb/pbc/lua/lobby_scene_service_lua.cpp"
    "src/pb/pbc/lua/lobby_scene_sol2.cpp"
    "src/pb/pbc/lua/login_service_service_lua.cpp"
    "src/pb/pbc/lua/login_service_sol2.cpp"
    "src/pb/pbc/lua/lua_service.cpp"
    "src/pb/pbc/lua/mission_comp_sol2.cpp"
    "src/pb/pbc/lua/mission_event_sol2.cpp"
    "src/pb/pbc/lua/node_common_sol2.cpp"
    "src/pb/pbc/lua/node_comp_sol2.cpp"
    "src/pb/pbc/lua/pb_sol2.cpp"
    "src/pb/pbc/lua/player_async_comp_sol2.cpp"
    "src/pb/pbc/lua/player_comp_sol2.cpp"
    "src/pb/pbc/lua/player_login_comp_sol2.cpp"
    "src/pb/pbc/lua/player_network_comp_sol2.cpp"
    "src/pb/pbc/lua/scene_client_player_service_lua.cpp"
    "src/pb/pbc/lua/scene_client_player_sol2.cpp"
    "src/pb/pbc/lua/scene_comp_sol2.cpp"
    "src/pb/pbc/lua/scene_event_sol2.cpp"
    "src/pb/pbc/lua/team_client_player_service_lua.cpp"
    "src/pb/pbc/lua/team_client_player_sol2.cpp"
    "src/pb/pbc/lua/team_comp_sol2.cpp"
    "src/pb/pbc/scene_client_player_service.cpp"
    "src/pb/pbc/scene_client_player_service.h"
    "src/pb/pbc/server_player_proto/controller_common_server_player.pb.cc"
    "src/pb/pbc/server_player_proto/controller_common_server_player.pb.h"
    "src/pb/pbc/server_player_proto/controller_scene_server_player.pb.cc"
    "src/pb/pbc/server_player_proto/controller_scene_server_player.pb.h"
    "src/pb/pbc/server_player_proto/game_scene_server_player.pb.cc"
    "src/pb/pbc/server_player_proto/game_scene_server_player.pb.h"
    "src/pb/pbc/server_player_proto/game_server_player.pb.cc"
    "src/pb/pbc/server_player_proto/game_server_player.pb.h"
    "src/pb/pbc/service.cpp"
    "src/pb/pbc/service.h"
    "src/pb/pbc/team_client_player_service.cpp"
    "src/pb/pbc/team_client_player_service.h"
    "src/redis_client/redis_client.cpp"
    "src/redis_client/redis_client.h"
    "src/util/defer.cpp"
    "src/util/defer.h"
    "src/util/file2string.cpp"
    "src/util/file2string.h"
    "src/util/game_registry.cpp"
    "src/util/game_registry.h"
    "src/util/random.h"
    "src/util/snow_flake.h"
)
source_group("" FILES ${no_group_source_files})

set(ALL_FILES
    ${no_group_source_files}
)

################################################################################
# Target
################################################################################
add_library(${PROJECT_NAME} STATIC ${ALL_FILES})

use_props(${PROJECT_NAME} "${CMAKE_CONFIGURATION_TYPES}" "${DEFAULT_CXX_PROPS}")
set(ROOT_NAMESPACE common)

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
            "${CMAKE_CURRENT_SOURCE_DIR}/src/pb/pbc;"
            "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/muduo;"
            "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/mysql/include;"
            "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/redis;"
            "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/muduo/contrib/windows/include;"
            "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/sol2/include;"
            "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/lua/src;"
            "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/redis/deps;"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/pb/pbc/common_proto;"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/pb/pbc/logic_proto;"
            "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/protobuf/third_party/abseil-cpp"
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
            "${CMAKE_CURRENT_SOURCE_DIR}/src/pb/pbc;"
            "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/muduo;"
            "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/mysql/include;"
            "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/redis;"
            "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/muduo/contrib/windows/include;"
            "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/sol2/include;"
            "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/lua/src;"
            "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/redis/deps;"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/pb/pbc/common_proto;"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/pb/pbc/logic_proto;"
            "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/protobuf/third_party/abseil-cpp"
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
            "NOMINMAX;"
            "GOOGLE_CHECK_EQ=ABSL_CHECK_EQ;"
            "GOOGLE_LOG=ABSL_LOG;"
            "GOOGLE_DCHECK=ABSL_DCHECK"
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
            "WIN32;"
            "_CRT_SECURE_NO_WARNINGS;"
            "_WINSOCK_DEPRECATED_NO_WARNINGS;"
            "_CRT_SECURE_NO_DEPRECATE;"
            "_CRT_NONSTDC_NO_DEPRECATE;"
            "ENTT_ID_TYPE=uint64_t;"
            "NOMINMAX;"
            "GOOGLE_CHECK_EQ=ABSL_CHECK_EQ;"
            "GOOGLE_LOG=ABSL_LOG;"
            "GOOGLE_DCHECK=ABSL_DCHECK"
        ">"
        "$<$<CONFIG:Release>:"
            "NDEBUG;"
            "_CONSOLE"
        ">"
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
                /MP;
                /WX;
                /wd4200;
                /wd4819;
                /wd4146;
                /bigobj;
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
            $<$<CONFIG:Debug>:
                /MP;
                /WX;
                /wd4200;
                /wd4819;
                /wd4146;
                /bigobj;
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
    endif()
    if("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
        target_link_options(${PROJECT_NAME} PRIVATE
            $<$<CONFIG:Release>:
                /DEBUG;
                /OPT:REF;
                /OPT:ICF;
                /INCREMENTAL:NO
            >
            /SUBSYSTEM:CONSOLE
        )
    elseif("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x86")
        target_link_options(${PROJECT_NAME} PRIVATE
            $<$<CONFIG:Release>:
                /DEBUG;
                /OPT:REF;
                /OPT:ICF;
                /INCREMENTAL:NO
            >
            /SUBSYSTEM:CONSOLE
        )
    endif()
endif()

################################################################################
# Dependencies
################################################################################
add_dependencies(${PROJECT_NAME}
    third_party
)

