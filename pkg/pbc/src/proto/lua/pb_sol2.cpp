#include <google/protobuf/message.h>
#include <sol/sol.hpp>
#include "thread_local/storage_lua.h"

void Pb2sol2c2gate();
void Pb2sol2centre_service();
void Pb2sol2common();
void Pb2sol2common_message();
void Pb2sol2comp();
void Pb2sol2empty();
void Pb2sol2game_service();
void Pb2sol2gate_service();
void Pb2sol2login_service();
void Pb2sol2session();
void Pb2sol2statistics();
void Pb2sol2tip();
void Pb2sol2user_accounts();
void Pb2sol2actor_comp();
void Pb2sol2actor_status_comp();
void Pb2sol2buff_comp();
void Pb2sol2frame_comp();
void Pb2sol2game_node_comp();
void Pb2sol2item_base_comp();
void Pb2sol2item_comp();
void Pb2sol2mission_comp();
void Pb2sol2npc_comp();
void Pb2sol2player_async_comp();
void Pb2sol2player_comp();
void Pb2sol2player_login_comp();
void Pb2sol2player_network_comp();
void Pb2sol2player_scene_comp();
void Pb2sol2player_skill_comp();
void Pb2sol2scene_comp();
void Pb2sol2skill_comp();
void Pb2sol2team_comp();
void Pb2sol2time_comp();
void Pb2sol2actor_combat_state_event();
void Pb2sol2actor_event();
void Pb2sol2buff_event();
void Pb2sol2combat_event();
void Pb2sol2mission_event();
void Pb2sol2npc_event();
void Pb2sol2player_event();
void Pb2sol2scene_event();
void Pb2sol2server_event();
void Pb2sol2skill_event();
void Pb2sol2centre_scene();
void Pb2sol2game_scene();
void Pb2sol2game_team();
void Pb2sol2player_common();
void Pb2sol2player_scene();
void Pb2sol2player_skill();
void Pb2sol2player_state_attribute_sync();
void Pb2sol2centre_player();
void Pb2sol2centre_player_scene();
void Pb2sol2game_player();
void Pb2sol2game_player_scene();
void Pb2sol2function_switch();
void Pb2sol2node();
void Pb2sol2scene();
void Pb2sol2test_switch();

void pb2sol2()
{
tls_lua_state.new_usertype<::google::protobuf::Message>("Message");
Pb2sol2c2gate();
Pb2sol2centre_service();
Pb2sol2common();
Pb2sol2common_message();
Pb2sol2comp();
Pb2sol2empty();
Pb2sol2game_service();
Pb2sol2gate_service();
Pb2sol2login_service();
Pb2sol2session();
Pb2sol2statistics();
Pb2sol2tip();
Pb2sol2user_accounts();
Pb2sol2actor_comp();
Pb2sol2actor_status_comp();
Pb2sol2buff_comp();
Pb2sol2frame_comp();
Pb2sol2game_node_comp();
Pb2sol2item_base_comp();
Pb2sol2item_comp();
Pb2sol2mission_comp();
Pb2sol2npc_comp();
Pb2sol2player_async_comp();
Pb2sol2player_comp();
Pb2sol2player_login_comp();
Pb2sol2player_network_comp();
Pb2sol2player_scene_comp();
Pb2sol2player_skill_comp();
Pb2sol2scene_comp();
Pb2sol2skill_comp();
Pb2sol2team_comp();
Pb2sol2time_comp();
Pb2sol2actor_combat_state_event();
Pb2sol2actor_event();
Pb2sol2buff_event();
Pb2sol2combat_event();
Pb2sol2mission_event();
Pb2sol2npc_event();
Pb2sol2player_event();
Pb2sol2scene_event();
Pb2sol2server_event();
Pb2sol2skill_event();
Pb2sol2centre_scene();
Pb2sol2game_scene();
Pb2sol2game_team();
Pb2sol2player_common();
Pb2sol2player_scene();
Pb2sol2player_skill();
Pb2sol2player_state_attribute_sync();
Pb2sol2centre_player();
Pb2sol2centre_player_scene();
Pb2sol2game_player();
Pb2sol2game_player_scene();
Pb2sol2function_switch();
Pb2sol2node();
Pb2sol2scene();
Pb2sol2test_switch();

}
