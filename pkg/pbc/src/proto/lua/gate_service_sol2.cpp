#include "common/gate_service.pb.h"
#include <sol/sol.hpp>
#include "thread_local/storage_lua.h"
void Pb2sol2gate_service()
{
tls_lua_state.new_usertype<KickSessionRequest>("KickSessionRequest",
"session_id",
sol::property(&KickSessionRequest::session_id, &KickSessionRequest::set_session_id),
"DebugString",
&KickSessionRequest::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<BroadcastToPlayersRequest>("BroadcastToPlayersRequest",
"add_session_list",
&BroadcastToPlayersRequest::add_session_list,
"session_list",
[](const BroadcastToPlayersRequest& pb, int index) ->decltype(auto){ return pb.session_list(index);},
"set_session_list",
[](BroadcastToPlayersRequest& pb, int index, uint64_t value) ->decltype(auto){ return pb.set_session_list(index, value);},
"session_list_size",
&BroadcastToPlayersRequest::session_list_size,
"clear_session_list",
&BroadcastToPlayersRequest::clear_session_list,
"body",
[](BroadcastToPlayersRequest& pb) ->decltype(auto){ return pb.body();},
"mutable_body",
[](BroadcastToPlayersRequest& pb) ->decltype(auto){ return pb.mutable_body();},
"DebugString",
&BroadcastToPlayersRequest::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

}
