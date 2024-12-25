#include "common/centre_service.pb.h"
#include <sol/sol.hpp>
#include "thread_local/storage_lua.h"
void Pb2sol2centre_service()
{
tls_lua_state.new_usertype<GateClientMessageRequest>("GateClientMessageRequest",
"rpc_client",
[](GateClientMessageRequest& pb) ->decltype(auto){ return pb.rpc_client();},
"mutable_rpc_client",
[](GateClientMessageRequest& pb) ->decltype(auto){ return pb.mutable_rpc_client();},
"DebugString",
&GateClientMessageRequest::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<LoginNodeLeaveGameRequest>("LoginNodeLeaveGameRequest",
"DebugString",
&LoginNodeLeaveGameRequest::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<EnterGameNodeSuccessRequest>("EnterGameNodeSuccessRequest",
"player_id",
sol::property(&EnterGameNodeSuccessRequest::player_id, &EnterGameNodeSuccessRequest::set_player_id),
"game_node_id",
sol::property(&EnterGameNodeSuccessRequest::game_node_id, &EnterGameNodeSuccessRequest::set_game_node_id),
"DebugString",
&EnterGameNodeSuccessRequest::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<CentrePlayerGameNodeEntryRequest>("CentrePlayerGameNodeEntryRequest",
"client_msg_body",
[](CentrePlayerGameNodeEntryRequest& pb) ->decltype(auto){ return pb.client_msg_body();},
"mutable_client_msg_body",
[](CentrePlayerGameNodeEntryRequest& pb) ->decltype(auto){ return pb.mutable_client_msg_body();},
"session_info",
[](CentrePlayerGameNodeEntryRequest& pb) ->decltype(auto){ return pb.session_info();},
"mutable_session_info",
[](CentrePlayerGameNodeEntryRequest& pb) ->decltype(auto){ return pb.mutable_session_info();},
"DebugString",
&CentrePlayerGameNodeEntryRequest::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

}
