#include "logic/client_player/player_scene.pb.h"
#include <sol/sol.hpp>
#include "thread_local/storage_lua.h"
void Pb2sol2player_scene()
{
tls_lua_state.new_usertype<EnterSceneC2SRequest>("EnterSceneC2SRequest",
"scene_info",
[](EnterSceneC2SRequest& pb) ->decltype(auto){ return pb.scene_info();},
"mutable_scene_info",
[](EnterSceneC2SRequest& pb) ->decltype(auto){ return pb.mutable_scene_info();},
"DebugString",
&EnterSceneC2SRequest::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<EnterSceneC2SResponse>("EnterSceneC2SResponse",
tls_lua_state.new_usertype<error_message>("error_message",
"DebugString",
&error_message::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<EnterSceneS2C>("EnterSceneS2C",
"scene_info",
[](EnterSceneS2C& pb) ->decltype(auto){ return pb.scene_info();},
"mutable_scene_info",
[](EnterSceneS2C& pb) ->decltype(auto){ return pb.mutable_scene_info();},
"DebugString",
&EnterSceneS2C::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<SceneInfoS2C>("SceneInfoS2C",
"add_scene_info",
&SceneInfoS2C::add_scene_info,
"scene_info",
[](const SceneInfoS2C& pb, int index) ->decltype(auto){ return pb.scene_info(index);},
"mutable_scene_info",
[](SceneInfoS2C& pb, int index) ->decltype(auto){ return pb.mutable_scene_info(index);},
"scene_info_size",
&SceneInfoS2C::scene_info_size,
"clear_scene_info",
&SceneInfoS2C::clear_scene_info,
"DebugString",
&SceneInfoS2C::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<SceneInfoRequest>("SceneInfoRequest",
"DebugString",
&SceneInfoRequest::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<SceneInfoResponse>("SceneInfoResponse",
"add_scene_info",
&SceneInfoResponse::add_scene_info,
"scene_info",
[](const SceneInfoResponse& pb, int index) ->decltype(auto){ return pb.scene_info(index);},
"mutable_scene_info",
[](SceneInfoResponse& pb, int index) ->decltype(auto){ return pb.mutable_scene_info(index);},
"scene_info_size",
&SceneInfoResponse::scene_info_size,
"clear_scene_info",
&SceneInfoResponse::clear_scene_info,
"DebugString",
&SceneInfoResponse::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<ActorCreateS2C>("ActorCreateS2C",
"entity",
sol::property(&ActorCreateS2C::entity, &ActorCreateS2C::set_entity),
"transform",
[](ActorCreateS2C& pb) ->decltype(auto){ return pb.transform();},
"mutable_transform",
[](ActorCreateS2C& pb) ->decltype(auto){ return pb.mutable_transform();},
"entity_type",
sol::property(&ActorCreateS2C::entity_type, &ActorCreateS2C::set_entity_type),
"guid",
sol::property(&ActorCreateS2C::guid, &ActorCreateS2C::set_guid),
"config_id",
sol::property(&ActorCreateS2C::config_id, &ActorCreateS2C::set_config_id),
"DebugString",
&ActorCreateS2C::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<ActorDestroyS2C>("ActorDestroyS2C",
"entity",
sol::property(&ActorDestroyS2C::entity, &ActorDestroyS2C::set_entity),
"DebugString",
&ActorDestroyS2C::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<ActorListCreateS2C>("ActorListCreateS2C",
"add_actor_list",
&ActorListCreateS2C::add_actor_list,
"actor_list",
[](const ActorListCreateS2C& pb, int index) ->decltype(auto){ return pb.actor_list(index);},
"mutable_actor_list",
[](ActorListCreateS2C& pb, int index) ->decltype(auto){ return pb.mutable_actor_list(index);},
"actor_list_size",
&ActorListCreateS2C::actor_list_size,
"clear_actor_list",
&ActorListCreateS2C::clear_actor_list,
"DebugString",
&ActorListCreateS2C::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<ActorListDestroyS2C>("ActorListDestroyS2C",
"add_entity",
&ActorListDestroyS2C::add_entity,
"entity",
[](const ActorListDestroyS2C& pb, int index) ->decltype(auto){ return pb.entity(index);},
"set_entity",
[](ActorListDestroyS2C& pb, int index, uint64_t value) ->decltype(auto){ return pb.set_entity(index, value);},
"entity_size",
&ActorListDestroyS2C::entity_size,
"clear_entity",
&ActorListDestroyS2C::clear_entity,
"DebugString",
&ActorListDestroyS2C::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

}
