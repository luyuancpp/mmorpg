#include "common/common_message.pb.h"
#include <sol/sol.hpp>
#include "thread_local/storage_lua.h"
void Pb2sol2common_message()
{
tls_lua_state.new_usertype<RoutingNodeInfo>("RoutingNodeInfo",
"node_info",
[](RoutingNodeInfo& pb) ->decltype(auto){ return pb.node_info();},
"mutable_node_info",
[](RoutingNodeInfo& pb) ->decltype(auto){ return pb.mutable_node_info();},
"message_id",
sol::property(&RoutingNodeInfo::message_id, &RoutingNodeInfo::set_message_id),
"DebugString",
&RoutingNodeInfo::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<MessageContent>("MessageContent",
"body",
[](MessageContent& pb) ->decltype(auto){ return pb.body();},
"mutable_body",
[](MessageContent& pb) ->decltype(auto){ return pb.mutable_body();},
"message_id",
sol::property(&MessageContent::message_id, &MessageContent::set_message_id),
"id",
sol::property(&MessageContent::id, &MessageContent::set_id),
tls_lua_state.new_usertype<>("",
"DebugString",
&::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<NodeMessageHeader>("NodeMessageHeader",
"node_id",
sol::property(&NodeMessageHeader::node_id, &NodeMessageHeader::set_node_id),
"session_id",
sol::property(&NodeMessageHeader::session_id, &NodeMessageHeader::set_session_id),
"DebugString",
&NodeMessageHeader::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<NodeRouteMessageRequest>("NodeRouteMessageRequest",
"body",
[](NodeRouteMessageRequest& pb) ->decltype(auto){ return pb.body();},
"mutable_body",
[](NodeRouteMessageRequest& pb) ->decltype(auto){ return pb.mutable_body();},
"header",
[](NodeRouteMessageRequest& pb) ->decltype(auto){ return pb.header();},
"mutable_header",
[](NodeRouteMessageRequest& pb) ->decltype(auto){ return pb.mutable_header();},
"DebugString",
&NodeRouteMessageRequest::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<NodeRouteMessageResponse>("NodeRouteMessageResponse",
"body",
[](NodeRouteMessageResponse& pb) ->decltype(auto){ return pb.body();},
"mutable_body",
[](NodeRouteMessageResponse& pb) ->decltype(auto){ return pb.mutable_body();},
"header",
[](NodeRouteMessageResponse& pb) ->decltype(auto){ return pb.header();},
"mutable_header",
[](NodeRouteMessageResponse& pb) ->decltype(auto){ return pb.mutable_header();},
"DebugString",
&NodeRouteMessageResponse::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<PlayerInfoExternal>("PlayerInfoExternal",
"player_id",
sol::property(&PlayerInfoExternal::player_id, &PlayerInfoExternal::set_player_id),
"DebugString",
&PlayerInfoExternal::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<PlayerNodeServiceRequest>("PlayerNodeServiceRequest",
"body",
[](PlayerNodeServiceRequest& pb) ->decltype(auto){ return pb.body();},
"mutable_body",
[](PlayerNodeServiceRequest& pb) ->decltype(auto){ return pb.mutable_body();},
"header",
[](PlayerNodeServiceRequest& pb) ->decltype(auto){ return pb.header();},
"mutable_header",
[](PlayerNodeServiceRequest& pb) ->decltype(auto){ return pb.mutable_header();},
"DebugString",
&PlayerNodeServiceRequest::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<PlayerMessageResponse>("PlayerMessageResponse",
"body",
[](PlayerMessageResponse& pb) ->decltype(auto){ return pb.body();},
"mutable_body",
[](PlayerMessageResponse& pb) ->decltype(auto){ return pb.mutable_body();},
"header",
[](PlayerMessageResponse& pb) ->decltype(auto){ return pb.header();},
"mutable_header",
[](PlayerMessageResponse& pb) ->decltype(auto){ return pb.mutable_header();},
"DebugString",
&PlayerMessageResponse::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<NodeRoutingMessageBody>("NodeRoutingMessageBody",
"node_info",
[](NodeRoutingMessageBody& pb) ->decltype(auto){ return pb.node_info();},
"mutable_node_info",
[](NodeRoutingMessageBody& pb) ->decltype(auto){ return pb.mutable_node_info();},
"DebugString",
&NodeRoutingMessageBody::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<RegisterGameNodeSessionRequest>("RegisterGameNodeSessionRequest",
"session_info",
[](RegisterGameNodeSessionRequest& pb) ->decltype(auto){ return pb.session_info();},
"mutable_session_info",
[](RegisterGameNodeSessionRequest& pb) ->decltype(auto){ return pb.mutable_session_info();},
"game_node_id",
sol::property(&RegisterGameNodeSessionRequest::game_node_id, &RegisterGameNodeSessionRequest::set_game_node_id),
"DebugString",
&RegisterGameNodeSessionRequest::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<RegisterGameNodeSessionResponse>("RegisterGameNodeSessionResponse",
"session_info",
[](RegisterGameNodeSessionResponse& pb) ->decltype(auto){ return pb.session_info();},
"mutable_session_info",
[](RegisterGameNodeSessionResponse& pb) ->decltype(auto){ return pb.mutable_session_info();},
"DebugString",
&RegisterGameNodeSessionResponse::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<RegisterGateNodeSessionResponse>("RegisterGateNodeSessionResponse",
"session_info",
[](RegisterGateNodeSessionResponse& pb) ->decltype(auto){ return pb.session_info();},
"mutable_session_info",
[](RegisterGateNodeSessionResponse& pb) ->decltype(auto){ return pb.mutable_session_info();},
"DebugString",
&RegisterGateNodeSessionResponse::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<RegisterCentreSessionNodeRequest>("RegisterCentreSessionNodeRequest",
"session_info",
[](RegisterCentreSessionNodeRequest& pb) ->decltype(auto){ return pb.session_info();},
"mutable_session_info",
[](RegisterCentreSessionNodeRequest& pb) ->decltype(auto){ return pb.mutable_session_info();},
"centre_node_id",
sol::property(&RegisterCentreSessionNodeRequest::centre_node_id, &RegisterCentreSessionNodeRequest::set_centre_node_id),
"DebugString",
&RegisterCentreSessionNodeRequest::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<RegisterSessionCentreNodeResponse>("RegisterSessionCentreNodeResponse",
"session_info",
[](RegisterSessionCentreNodeResponse& pb) ->decltype(auto){ return pb.session_info();},
"mutable_session_info",
[](RegisterSessionCentreNodeResponse& pb) ->decltype(auto){ return pb.mutable_session_info();},
"DebugString",
&RegisterSessionCentreNodeResponse::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<GateSessionDisconnectRequest>("GateSessionDisconnectRequest",
"session_info",
[](GateSessionDisconnectRequest& pb) ->decltype(auto){ return pb.session_info();},
"mutable_session_info",
[](GateSessionDisconnectRequest& pb) ->decltype(auto){ return pb.mutable_session_info();},
"DebugString",
&GateSessionDisconnectRequest::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<RouteMessageRequest>("RouteMessageRequest",
"body",
[](RouteMessageRequest& pb) ->decltype(auto){ return pb.body();},
"mutable_body",
[](RouteMessageRequest& pb) ->decltype(auto){ return pb.mutable_body();},
"add_route_nodes",
&RouteMessageRequest::add_route_nodes,
"route_nodes",
[](const RouteMessageRequest& pb, int index) ->decltype(auto){ return pb.route_nodes(index);},
"mutable_route_nodes",
[](RouteMessageRequest& pb, int index) ->decltype(auto){ return pb.mutable_route_nodes(index);},
"route_nodes_size",
&RouteMessageRequest::route_nodes_size,
"clear_route_nodes",
&RouteMessageRequest::clear_route_nodes,
"session_id",
sol::property(&RouteMessageRequest::session_id, &RouteMessageRequest::set_session_id),
"id",
sol::property(&RouteMessageRequest::id, &RouteMessageRequest::set_id),
"is_client",
sol::property(&RouteMessageRequest::is_client, &RouteMessageRequest::set_is_client),
"DebugString",
&RouteMessageRequest::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<RouteMessageResponse>("RouteMessageResponse",
"body",
[](RouteMessageResponse& pb) ->decltype(auto){ return pb.body();},
"mutable_body",
[](RouteMessageResponse& pb) ->decltype(auto){ return pb.mutable_body();},
"add_route_nodes",
&RouteMessageResponse::add_route_nodes,
"route_nodes",
[](const RouteMessageResponse& pb, int index) ->decltype(auto){ return pb.route_nodes(index);},
"mutable_route_nodes",
[](RouteMessageResponse& pb, int index) ->decltype(auto){ return pb.mutable_route_nodes(index);},
"route_nodes_size",
&RouteMessageResponse::route_nodes_size,
"clear_route_nodes",
&RouteMessageResponse::clear_route_nodes,
"session_id",
sol::property(&RouteMessageResponse::session_id, &RouteMessageResponse::set_session_id),
"id",
sol::property(&RouteMessageResponse::id, &RouteMessageResponse::set_id),
"is_client",
sol::property(&RouteMessageResponse::is_client, &RouteMessageResponse::set_is_client),
"DebugString",
&RouteMessageResponse::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<RoutePlayerMessageRequest>("RoutePlayerMessageRequest",
"body",
[](RoutePlayerMessageRequest& pb) ->decltype(auto){ return pb.body();},
"mutable_body",
[](RoutePlayerMessageRequest& pb) ->decltype(auto){ return pb.mutable_body();},
"add_node_list",
&RoutePlayerMessageRequest::add_node_list,
"node_list",
[](const RoutePlayerMessageRequest& pb, int index) ->decltype(auto){ return pb.node_list(index);},
"mutable_node_list",
[](RoutePlayerMessageRequest& pb, int index) ->decltype(auto){ return pb.mutable_node_list(index);},
"node_list_size",
&RoutePlayerMessageRequest::node_list_size,
"clear_node_list",
&RoutePlayerMessageRequest::clear_node_list,
"player_info",
[](RoutePlayerMessageRequest& pb) ->decltype(auto){ return pb.player_info();},
"mutable_player_info",
[](RoutePlayerMessageRequest& pb) ->decltype(auto){ return pb.mutable_player_info();},
"DebugString",
&RoutePlayerMessageRequest::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<RoutePlayerMessageResponse>("RoutePlayerMessageResponse",
"body",
[](RoutePlayerMessageResponse& pb) ->decltype(auto){ return pb.body();},
"mutable_body",
[](RoutePlayerMessageResponse& pb) ->decltype(auto){ return pb.mutable_body();},
"add_node_list",
&RoutePlayerMessageResponse::add_node_list,
"node_list",
[](const RoutePlayerMessageResponse& pb, int index) ->decltype(auto){ return pb.node_list(index);},
"mutable_node_list",
[](RoutePlayerMessageResponse& pb, int index) ->decltype(auto){ return pb.mutable_node_list(index);},
"node_list_size",
&RoutePlayerMessageResponse::node_list_size,
"clear_node_list",
&RoutePlayerMessageResponse::clear_node_list,
"player_info",
[](RoutePlayerMessageResponse& pb) ->decltype(auto){ return pb.player_info();},
"mutable_player_info",
[](RoutePlayerMessageResponse& pb) ->decltype(auto){ return pb.mutable_player_info();},
"DebugString",
&RoutePlayerMessageResponse::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<RegisterGameNodeRequest>("RegisterGameNodeRequest",
"rpc_client",
[](RegisterGameNodeRequest& pb) ->decltype(auto){ return pb.rpc_client();},
"mutable_rpc_client",
[](RegisterGameNodeRequest& pb) ->decltype(auto){ return pb.mutable_rpc_client();},
"rpc_server",
[](RegisterGameNodeRequest& pb) ->decltype(auto){ return pb.rpc_server();},
"mutable_rpc_server",
[](RegisterGameNodeRequest& pb) ->decltype(auto){ return pb.mutable_rpc_server();},
"game_node_id",
sol::property(&RegisterGameNodeRequest::game_node_id, &RegisterGameNodeRequest::set_game_node_id),
"server_type",
sol::property(&RegisterGameNodeRequest::server_type, &RegisterGameNodeRequest::set_server_type),
"DebugString",
&RegisterGameNodeRequest::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<RegisterGateNodeRequest>("RegisterGateNodeRequest",
"rpc_client",
[](RegisterGateNodeRequest& pb) ->decltype(auto){ return pb.rpc_client();},
"mutable_rpc_client",
[](RegisterGateNodeRequest& pb) ->decltype(auto){ return pb.mutable_rpc_client();},
"gate_node_id",
sol::property(&RegisterGateNodeRequest::gate_node_id, &RegisterGateNodeRequest::set_gate_node_id),
"DebugString",
&RegisterGateNodeRequest::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<UnregisterGameNodeRequest>("UnregisterGameNodeRequest",
"game_node_id",
sol::property(&UnregisterGameNodeRequest::game_node_id, &UnregisterGameNodeRequest::set_game_node_id),
"DebugString",
&UnregisterGameNodeRequest::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

}
