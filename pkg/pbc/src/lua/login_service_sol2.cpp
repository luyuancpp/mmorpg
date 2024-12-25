#include "common/login_service.pb.h"
#include <sol/sol.hpp>
#include "thread_local/storage_lua.h"
void Pb2sol2login_service()
{
tls_lua_state.new_usertype<LoginC2LRequest>("LoginC2LRequest",
"client_msg_body",
[](LoginC2LRequest& pb) ->decltype(auto){ return pb.client_msg_body();},
"mutable_client_msg_body",
[](LoginC2LRequest& pb) ->decltype(auto){ return pb.mutable_client_msg_body();},
"session_info",
[](LoginC2LRequest& pb) ->decltype(auto){ return pb.session_info();},
"mutable_session_info",
[](LoginC2LRequest& pb) ->decltype(auto){ return pb.mutable_session_info();},
"DebugString",
&LoginC2LRequest::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<LoginC2LResponse>("LoginC2LResponse",
"client_msg_body",
[](LoginC2LResponse& pb) ->decltype(auto){ return pb.client_msg_body();},
"mutable_client_msg_body",
[](LoginC2LResponse& pb) ->decltype(auto){ return pb.mutable_client_msg_body();},
"session_info",
[](LoginC2LResponse& pb) ->decltype(auto){ return pb.session_info();},
"mutable_session_info",
[](LoginC2LResponse& pb) ->decltype(auto){ return pb.mutable_session_info();},
"DebugString",
&LoginC2LResponse::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<CreatePlayerC2LRequest>("CreatePlayerC2LRequest",
"client_msg_body",
[](CreatePlayerC2LRequest& pb) ->decltype(auto){ return pb.client_msg_body();},
"mutable_client_msg_body",
[](CreatePlayerC2LRequest& pb) ->decltype(auto){ return pb.mutable_client_msg_body();},
"session_info",
[](CreatePlayerC2LRequest& pb) ->decltype(auto){ return pb.session_info();},
"mutable_session_info",
[](CreatePlayerC2LRequest& pb) ->decltype(auto){ return pb.mutable_session_info();},
"DebugString",
&CreatePlayerC2LRequest::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<EnterGameC2LRequest>("EnterGameC2LRequest",
"client_msg_body",
[](EnterGameC2LRequest& pb) ->decltype(auto){ return pb.client_msg_body();},
"mutable_client_msg_body",
[](EnterGameC2LRequest& pb) ->decltype(auto){ return pb.mutable_client_msg_body();},
"session_info",
[](EnterGameC2LRequest& pb) ->decltype(auto){ return pb.session_info();},
"mutable_session_info",
[](EnterGameC2LRequest& pb) ->decltype(auto){ return pb.mutable_session_info();},
"DebugString",
&EnterGameC2LRequest::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<CreatePlayerC2LResponse>("CreatePlayerC2LResponse",
"client_msg_body",
[](CreatePlayerC2LResponse& pb) ->decltype(auto){ return pb.client_msg_body();},
"mutable_client_msg_body",
[](CreatePlayerC2LResponse& pb) ->decltype(auto){ return pb.mutable_client_msg_body();},
"session_info",
[](CreatePlayerC2LResponse& pb) ->decltype(auto){ return pb.session_info();},
"mutable_session_info",
[](CreatePlayerC2LResponse& pb) ->decltype(auto){ return pb.mutable_session_info();},
"DebugString",
&CreatePlayerC2LResponse::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<EnterGameC2LResponse>("EnterGameC2LResponse",
"client_msg_body",
[](EnterGameC2LResponse& pb) ->decltype(auto){ return pb.client_msg_body();},
"mutable_client_msg_body",
[](EnterGameC2LResponse& pb) ->decltype(auto){ return pb.mutable_client_msg_body();},
"session_info",
[](EnterGameC2LResponse& pb) ->decltype(auto){ return pb.session_info();},
"mutable_session_info",
[](EnterGameC2LResponse& pb) ->decltype(auto){ return pb.mutable_session_info();},
"DebugString",
&EnterGameC2LResponse::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<LoginNodeDisconnectRequest>("LoginNodeDisconnectRequest",
"session_id",
sol::property(&LoginNodeDisconnectRequest::session_id, &LoginNodeDisconnectRequest::set_session_id),
"DebugString",
&LoginNodeDisconnectRequest::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<LeaveGameC2LRequest>("LeaveGameC2LRequest",
"session_id",
sol::property(&LeaveGameC2LRequest::session_id, &LeaveGameC2LRequest::set_session_id),
"DebugString",
&LeaveGameC2LRequest::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

}
