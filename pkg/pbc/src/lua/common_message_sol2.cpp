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
tls_lua_state.new_usertype<>("",
"message_id",
sol::property(&::message_id, &::set_message_id),
"id",
sol::property(&::id, &::set_id),
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
"message_content",
[](NodeRouteMessageRequest& pb) ->decltype(auto){ return pb.message_content();},
"mutable_message_content",
[](NodeRouteMessageRequest& pb) ->decltype(auto){ return pb.mutable_message_content();},
"header",
[](NodeRouteMessageRequest& pb) ->decltype(auto){ return pb.header();},
"mutable_header",
[](NodeRouteMessageRequest& pb) ->decltype(auto){ return pb.mutable_header();},
"DebugString",
&NodeRouteMessageRequest::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<NodeRouteMessageResponse>("NodeRouteMessageResponse",
"message_content",
[](NodeRouteMessageResponse& pb) ->decltype(auto){ return pb.message_content();},
"mutable_message_content",
[](NodeRouteMessageResponse& pb) ->decltype(auto){ return pb.mutable_message_content();},
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
"message_content",
[](PlayerNodeServiceRequest& pb) ->decltype(auto){ return pb.message_content();},
"mutable_message_content",
[](PlayerNodeServiceRequest& pb) ->decltype(auto){ return pb.mutable_message_content();},
"header",
[](PlayerNodeServiceRequest& pb) ->decltype(auto){ return pb.header();},
"mutable_header",
[](PlayerNodeServiceRequest& pb) ->decltype(auto){ return pb.mutable_header();},
"DebugString",
&PlayerNodeServiceRequest::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<PlayerMessageResponse>("PlayerMessageResponse",
"message_content",
[](PlayerMessageResponse& pb) ->decltype(auto){ return pb.message_content();},
"mutable_message_content",
[](PlayerMessageResponse& pb) ->decltype(auto){ return pb.mutable_message_content();},
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

tls_lua_state.new_usertype<ProtoFieldCheckerTestSubPB>("ProtoFieldCheckerTestSubPB",
"add_items",
[](ProtoFieldCheckerTestSubPB& pb, const std::string& value) ->decltype(auto){ return pb.add_items(value);},
"items",
[](ProtoFieldCheckerTestSubPB& pb, int index) ->decltype(auto){ return pb.items(index);},
"set_items",
[](ProtoFieldCheckerTestSubPB& pb, int index, const std::string& value) ->decltype(auto){ return pb.set_items(index, value);},
"items_size",
&ProtoFieldCheckerTestSubPB::items_size,
"clear_items",
&ProtoFieldCheckerTestSubPB::clear_items,
"DebugString",
&ProtoFieldCheckerTestSubPB::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<ProtoFieldCheckerTestPB>("ProtoFieldCheckerTestPB",
tls_lua_state.new_usertype<sub_message>("sub_message",
"DebugString",
&sub_message::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<MyProtoMessage>("MyProtoMessage",
"single_int32_field",
sol::property(&MyProtoMessage::single_int32_field, &MyProtoMessage::set_single_int32_field),
"single_int64_field",
sol::property(&MyProtoMessage::single_int64_field, &MyProtoMessage::set_single_int64_field),
"add_repeated_int32_field",
&MyProtoMessage::add_repeated_int32_field,
"repeated_int32_field",
[](const MyProtoMessage& pb, int index) ->decltype(auto){ return pb.repeated_int32_field(index);},
"set_repeated_int32_field",
[](MyProtoMessage& pb, int index, int32_t value) ->decltype(auto){ return pb.set_repeated_int32_field(index, value);},
"repeated_int32_field_size",
&MyProtoMessage::repeated_int32_field_size,
"clear_repeated_int32_field",
&MyProtoMessage::clear_repeated_int32_field,
"add_repeated_int64_field",
&MyProtoMessage::add_repeated_int64_field,
"repeated_int64_field",
[](const MyProtoMessage& pb, int index) ->decltype(auto){ return pb.repeated_int64_field(index);},
"set_repeated_int64_field",
[](MyProtoMessage& pb, int index, int64_t value) ->decltype(auto){ return pb.set_repeated_int64_field(index, value);},
"repeated_int64_field_size",
&MyProtoMessage::repeated_int64_field_size,
"clear_repeated_int64_field",
&MyProtoMessage::clear_repeated_int64_field,
tls_lua_state.new_usertype<>("",
"DebugString",
&::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<MyNestedMessage>("MyNestedMessage",
"nested_int32_field",
sol::property(&MyNestedMessage::nested_int32_field, &MyNestedMessage::set_nested_int32_field),
"nested_int64_field",
sol::property(&MyNestedMessage::nested_int64_field, &MyNestedMessage::set_nested_int64_field),
"DebugString",
&MyNestedMessage::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

}
