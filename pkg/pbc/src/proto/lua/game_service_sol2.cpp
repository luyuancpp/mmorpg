#include "common/game_service.pb.h"
#include <sol/sol.hpp>
#include "thread_local/storage_lua.h"
void Pb2sol2game_service()
{
tls_lua_state.new_usertype<PlayerEnterGameNodeRequest>("PlayerEnterGameNodeRequest",
"player_id",
sol::property(&PlayerEnterGameNodeRequest::player_id, &PlayerEnterGameNodeRequest::set_player_id),
"session_id",
sol::property(&PlayerEnterGameNodeRequest::session_id, &PlayerEnterGameNodeRequest::set_session_id),
"centre_node_id",
sol::property(&PlayerEnterGameNodeRequest::centre_node_id, &PlayerEnterGameNodeRequest::set_centre_node_id),
"DebugString",
&PlayerEnterGameNodeRequest::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<ClientSendMessageToPlayerRequest>("ClientSendMessageToPlayerRequest",
"body",
[](ClientSendMessageToPlayerRequest& pb) ->decltype(auto){ return pb.body();},
"mutable_body",
[](ClientSendMessageToPlayerRequest& pb) ->decltype(auto){ return pb.mutable_body();},
"session_id",
sol::property(&ClientSendMessageToPlayerRequest::session_id, &ClientSendMessageToPlayerRequest::set_session_id),
"message_id",
sol::property(&ClientSendMessageToPlayerRequest::message_id, &ClientSendMessageToPlayerRequest::set_message_id),
"id",
sol::property(&ClientSendMessageToPlayerRequest::id, &ClientSendMessageToPlayerRequest::set_id),
"DebugString",
&ClientSendMessageToPlayerRequest::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<ClientSendMessageToPlayerResponse>("ClientSendMessageToPlayerResponse",
"response",
[](ClientSendMessageToPlayerResponse& pb) ->decltype(auto){ return pb.response();},
"mutable_response",
[](ClientSendMessageToPlayerResponse& pb) ->decltype(auto){ return pb.mutable_response();},
"session_id",
sol::property(&ClientSendMessageToPlayerResponse::session_id, &ClientSendMessageToPlayerResponse::set_session_id),
"message_id",
sol::property(&ClientSendMessageToPlayerResponse::message_id, &ClientSendMessageToPlayerResponse::set_message_id),
"id",
sol::property(&ClientSendMessageToPlayerResponse::id, &ClientSendMessageToPlayerResponse::set_id),
"DebugString",
&ClientSendMessageToPlayerResponse::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<SessionDisconnectRequest>("SessionDisconnectRequest",
"player_id",
sol::property(&SessionDisconnectRequest::player_id, &SessionDisconnectRequest::set_player_id),
"DebugString",
&SessionDisconnectRequest::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<GameNodeConnectRequest>("GameNodeConnectRequest",
"rpc_client",
[](GameNodeConnectRequest& pb) ->decltype(auto){ return pb.rpc_client();},
"mutable_rpc_client",
[](GameNodeConnectRequest& pb) ->decltype(auto){ return pb.mutable_rpc_client();},
"gate_node_id",
sol::property(&GameNodeConnectRequest::gate_node_id, &GameNodeConnectRequest::set_gate_node_id),
"DebugString",
&GameNodeConnectRequest::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<RegisterPlayerSessionRequest>("RegisterPlayerSessionRequest",
"session_id",
sol::property(&RegisterPlayerSessionRequest::session_id, &RegisterPlayerSessionRequest::set_session_id),
"player_id",
sol::property(&RegisterPlayerSessionRequest::player_id, &RegisterPlayerSessionRequest::set_player_id),
"DebugString",
&RegisterPlayerSessionRequest::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<Centre2GsEnterSceneRequest>("Centre2GsEnterSceneRequest",
"player_id",
sol::property(&Centre2GsEnterSceneRequest::player_id, &Centre2GsEnterSceneRequest::set_player_id),
"scene_id",
sol::property(&Centre2GsEnterSceneRequest::scene_id, &Centre2GsEnterSceneRequest::set_scene_id),
"DebugString",
&Centre2GsEnterSceneRequest::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<CreateSceneRequest>("CreateSceneRequest",
"config_id",
sol::property(&CreateSceneRequest::config_id, &CreateSceneRequest::set_config_id),
"DebugString",
&CreateSceneRequest::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<CreateSceneResponse>("CreateSceneResponse",
"scene_info",
[](CreateSceneResponse& pb) ->decltype(auto){ return pb.scene_info();},
"mutable_scene_info",
[](CreateSceneResponse& pb) ->decltype(auto){ return pb.mutable_scene_info();},
"DebugString",
&CreateSceneResponse::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

}
