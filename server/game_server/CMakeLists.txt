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
    "src/handler/game_scene_handler.h"
    "src/handler/game_scene_server_player_handler.h"
    "src/handler/game_server_player_handler.h"
    "src/handler/game_service_handler.h"
    "src/handler/lobby_scene_handler.h"
    "src/handler/player_service.h"
    "src/handler/register_handler.h"
    "src/handler/scene_client_player_handler.h"
    "src/handler/team_client_player_handler.h"
    "src/network/controller_node.h"
    "src/network/message_system.h"
    "src/replied_handler/common_client_player_replied_handler.h"
    "src/replied_handler/controller_common_server_player_replied_handler.h"
    "src/replied_handler/controller_scene_replied_handler.h"
    "src/replied_handler/controller_scene_server_player_replied_handler.h"
    "src/replied_handler/controller_service_replied_handler.h"
    "src/replied_handler/deploy_service_replied_handler.h"
    "src/replied_handler/lobby_scene_replied_handler.h"
    "src/replied_handler/player_service_replied.h"
    "src/replied_handler/scene_client_player_replied_handler.h"
    "src/replied_handler/team_client_player_replied_handler.h"
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
    "src/handler/game_scene_handler.cpp"
    "src/handler/game_scene_server_player_handler.cpp"
    "src/handler/game_server_player_handler.cpp"
    "src/handler/game_service_handler.cpp"
    "src/handler/lobby_scene_handler.cpp"
    "src/handler/player_service.cpp"
    "src/handler/register_handler.cpp"
    "src/handler/scene_client_player_handler.cpp"
    "src/handler/team_client_player_handler.cpp"
    "src/network/message_system.cpp"
    "src/replied_handler/common_client_player_replied_handler.cpp"
    "src/replied_handler/controller_common_server_player_replied_handler.cpp"
    "src/replied_handler/controller_scene_replied_handler.cpp"
    "src/replied_handler/controller_scene_server_player_replied_handler.cpp"
    "src/replied_handler/controller_service_replied_handler.cpp"
    "src/replied_handler/deploy_service_replied_handler.cpp"
    "src/replied_handler/lobby_scene_replied_handler.cpp"
    "src/replied_handler/player_service_replied.cpp"
    "src/replied_handler/register_replied_handler.cpp"
    "src/replied_handler/scene_client_player_replied_handler.cpp"
    "src/replied_handler/team_client_player_replied_handler.cpp"
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
if("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
    string(CONCAT "MSVC_RUNTIME_LIBRARY_STR"
        $<$<CONFIG:Debug>:
            MultiThreadedDebugDLL
        >
        $<$<NOT:$<OR:$<CONFIG:Debug>>>:${MSVC_RUNTIME_LIBRARY_DEFAULT}>
    )
elseif("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x86")
    string(CONCAT "MSVC_RUNTIME_LIBRARY_STR"
        $<$<CONFIG:Debug>:
            MultiThreadedDebugDLL
        >
        $<$<NOT:$<OR:$<CONFIG:Debug>>>:${MSVC_RUNTIME_LIBRARY_DEFAULT}>
    )
endif()
set_target_properties(${PROJECT_NAME} PROPERTIES MSVC_RUNTIME_LIBRARY ${MSVC_RUNTIME_LIBRARY_STR})

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
            "${CMAKE_CURRENT_SOURCE_DIR}/../common/src/pb/pbc/common_proto;"
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
            "${CMAKE_CURRENT_SOURCE_DIR}/../common;"
            "${CMAKE_CURRENT_SOURCE_DIR}/../common/src/pb/pbc;"
            "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/redis;"
            "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/muduo;"
            "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/muduo/contrib/windows/include;"
            "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/redis/deps;"
            "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/ue5navmesh/Public;"
            "${CMAKE_CURRENT_SOURCE_DIR}/../common/src/pb/pbc/common_proto;"
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
            "_TCHAR_DEFINED;"
            "UNICODE;"
            "UNICODE;"
            "_UNICODE"
        ">"
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
            "_TCHAR_DEFINED;"
            "UNICODE;"
            "UNICODE;"
            "_UNICODE"
        ">"
    )
endif()

################################################################################
# Compile and link options
################################################################################
if(MSVC)
    if("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
        target_compile_options(${PROJECT_NAME} PRIVATE
            $<$<CONFIG:Debug>:
                /permissive-;
                /MP;
                /sdl;
                /W3;
                /WX;
                /wd4200;
                /wd4819;
                /wd4146;
                /utf-8
            >
            ${DEFAULT_CXX_DEBUG_INFORMATION_FORMAT};
            ${DEFAULT_CXX_EXCEPTION_HANDLING}
        )
    elseif("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x86")
        target_compile_options(${PROJECT_NAME} PRIVATE
            $<$<CONFIG:Debug>:
                /permissive-;
                /MP;
                /sdl;
                /W3;
                /WX;
                /wd4200;
                /wd4819;
                /wd4146;
                /utf-8
            >
            ${DEFAULT_CXX_DEBUG_INFORMATION_FORMAT};
            ${DEFAULT_CXX_EXCEPTION_HANDLING}
        )
    endif()
    if("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
        target_link_options(${PROJECT_NAME} PRIVATE
            $<$<CONFIG:Debug>:
                /MANIFEST:NO;
                /DEBUG;
                /SUBSYSTEM:CONSOLE
            >
            $<$<CONFIG:Release>:
                /DEBUG:FULL
            >
        )
    elseif("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x86")
        target_link_options(${PROJECT_NAME} PRIVATE
            $<$<CONFIG:Debug>:
                /MANIFEST:NO;
                /DEBUG;
                /SUBSYSTEM:CONSOLE
            >
            $<$<CONFIG:Release>:
                /DEBUG:FULL
            >
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
            "libprotobuf;"
            "zlib;"
            "hiredis;"
            "absl_bad_any_cast_impl;"
            "absl_bad_optional_access;"
            "absl_bad_variant_access;"
            "absl_base;"
            "absl_city;"
            "absl_civil_time;"
            "absl_cord;"
            "absl_cordz_functions;"
            "absl_cordz_handle;"
            "absl_cordz_info;"
            "absl_cordz_sample_token;"
            "absl_cord_internal;"
            "absl_crc32c;"
            "absl_crc_cord_state;"
            "absl_crc_cpu_detect;"
            "absl_crc_internal;"
            "absl_debugging_internal;"
            "absl_demangle_internal;"
            "absl_die_if_null;"
            "absl_examine_stack;"
            "absl_exponential_biased;"
            "absl_failure_signal_handler;"
            "absl_flags;"
            "absl_flags_commandlineflag;"
            "absl_flags_commandlineflag_internal;"
            "absl_flags_config;"
            "absl_flags_internal;"
            "absl_flags_marshalling;"
            "absl_flags_parse;"
            "absl_flags_private_handle_accessor;"
            "absl_flags_program_name;"
            "absl_flags_reflection;"
            "absl_flags_usage;"
            "absl_flags_usage_internal;"
            "absl_graphcycles_internal;"
            "absl_hash;"
            "absl_hashtablez_sampler;"
            "absl_int128;"
            "absl_leak_check;"
            "absl_log_entry;"
            "absl_log_flags;"
            "absl_log_globals;"
            "absl_log_initialize;"
            "absl_log_internal_check_op;"
            "absl_log_internal_conditions;"
            "absl_log_internal_format;"
            "absl_log_internal_globals;"
            "absl_log_internal_log_sink_set;"
            "absl_log_internal_message;"
            "absl_log_internal_nullguard;"
            "absl_log_internal_proto;"
            "absl_log_severity;"
            "absl_log_sink;"
            "absl_low_level_hash;"
            "absl_malloc_internal;"
            "absl_periodic_sampler;"
            "absl_random_distributions;"
            "absl_random_internal_distribution_test_util;"
            "absl_random_internal_platform;"
            "absl_random_internal_pool_urbg;"
            "absl_random_internal_randen;"
            "absl_random_internal_randen_hwaes;"
            "absl_random_internal_randen_hwaes_impl;"
            "absl_random_internal_randen_slow;"
            "absl_random_internal_seed_material;"
            "absl_random_seed_gen_exception;"
            "absl_random_seed_sequences;"
            "absl_raw_hash_set;"
            "absl_raw_logging_internal;"
            "absl_scoped_mock_log;"
            "absl_scoped_set_env;"
            "absl_spinlock_wait;"
            "absl_stacktrace;"
            "absl_status;"
            "absl_statusor;"
            "absl_strerror;"
            "absl_strings;"
            "absl_strings_internal;"
            "absl_str_format_internal;"
            "absl_symbolize;"
            "absl_synchronization;"
            "absl_throw_delegate;"
            "absl_time;"
            "absl_time_zone;"
            "libprotobuf-lite;"
            "libprotoc;"
            "gmock;"
            "gmock_main;"
            "utf8_range;"
            "utf8_validity"
        ">"
    )
elseif("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x86")
    set(ADDITIONAL_LIBRARY_DEPENDENCIES
        "$<$<CONFIG:Debug>:"
            "third_party;"
            "common;"
            "libprotobuf;"
            "zlib;"
            "hiredis;"
            "absl_bad_any_cast_impl;"
            "absl_bad_optional_access;"
            "absl_bad_variant_access;"
            "absl_base;"
            "absl_city;"
            "absl_civil_time;"
            "absl_cord;"
            "absl_cordz_functions;"
            "absl_cordz_handle;"
            "absl_cordz_info;"
            "absl_cordz_sample_token;"
            "absl_cord_internal;"
            "absl_crc32c;"
            "absl_crc_cord_state;"
            "absl_crc_cpu_detect;"
            "absl_crc_internal;"
            "absl_debugging_internal;"
            "absl_demangle_internal;"
            "absl_die_if_null;"
            "absl_examine_stack;"
            "absl_exponential_biased;"
            "absl_failure_signal_handler;"
            "absl_flags;"
            "absl_flags_commandlineflag;"
            "absl_flags_commandlineflag_internal;"
            "absl_flags_config;"
            "absl_flags_internal;"
            "absl_flags_marshalling;"
            "absl_flags_parse;"
            "absl_flags_private_handle_accessor;"
            "absl_flags_program_name;"
            "absl_flags_reflection;"
            "absl_flags_usage;"
            "absl_flags_usage_internal;"
            "absl_graphcycles_internal;"
            "absl_hash;"
            "absl_hashtablez_sampler;"
            "absl_int128;"
            "absl_leak_check;"
            "absl_log_entry;"
            "absl_log_flags;"
            "absl_log_globals;"
            "absl_log_initialize;"
            "absl_log_internal_check_op;"
            "absl_log_internal_conditions;"
            "absl_log_internal_format;"
            "absl_log_internal_globals;"
            "absl_log_internal_log_sink_set;"
            "absl_log_internal_message;"
            "absl_log_internal_nullguard;"
            "absl_log_internal_proto;"
            "absl_log_severity;"
            "absl_log_sink;"
            "absl_low_level_hash;"
            "absl_malloc_internal;"
            "absl_periodic_sampler;"
            "absl_random_distributions;"
            "absl_random_internal_distribution_test_util;"
            "absl_random_internal_platform;"
            "absl_random_internal_pool_urbg;"
            "absl_random_internal_randen;"
            "absl_random_internal_randen_hwaes;"
            "absl_random_internal_randen_hwaes_impl;"
            "absl_random_internal_randen_slow;"
            "absl_random_internal_seed_material;"
            "absl_random_seed_gen_exception;"
            "absl_random_seed_sequences;"
            "absl_raw_hash_set;"
            "absl_raw_logging_internal;"
            "absl_scoped_mock_log;"
            "absl_scoped_set_env;"
            "absl_spinlock_wait;"
            "absl_stacktrace;"
            "absl_status;"
            "absl_statusor;"
            "absl_strerror;"
            "absl_strings;"
            "absl_strings_internal;"
            "absl_str_format_internal;"
            "absl_symbolize;"
            "absl_synchronization;"
            "absl_throw_delegate;"
            "absl_time;"
            "absl_time_zone;"
            "libprotobuf-lite;"
            "libprotoc;"
            "gmock;"
            "gmock_main;"
            "utf8_range;"
            "utf8_validity"
        ">"
    )
endif()
target_link_libraries(${PROJECT_NAME} PRIVATE "${ADDITIONAL_LIBRARY_DEPENDENCIES}")

if("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
    target_link_directories(${PROJECT_NAME} PRIVATE
        "$<$<CONFIG:Debug>:"
            "${CMAKE_CURRENT_SOURCE_DIR}/../bin/;"
            "${CMAKE_CURRENT_SOURCE_DIR}/../lib/"
        ">"
    )
elseif("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x86")
    target_link_directories(${PROJECT_NAME} PRIVATE
        "$<$<CONFIG:Debug>:"
            "${CMAKE_CURRENT_SOURCE_DIR}/../bin/;"
            "${CMAKE_CURRENT_SOURCE_DIR}/../lib/"
        ">"
    )
endif()

