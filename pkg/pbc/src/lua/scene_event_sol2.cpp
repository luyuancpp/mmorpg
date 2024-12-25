#include "logic/event/scene_event.pb.h"
#include <sol/sol.hpp>
#include "thread_local/storage_lua.h"
void Pb2sol2scene_event()
{
tls_lua_state.new_usertype<OnSceneCreate>("OnSceneCreate",
"entity",
sol::property(&OnSceneCreate::entity, &OnSceneCreate::set_entity),
"DebugString",
&OnSceneCreate::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<OnDestroyScene>("OnDestroyScene",
"entity",
sol::property(&OnDestroyScene::entity, &OnDestroyScene::set_entity),
"DebugString",
&OnDestroyScene::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<BeforeEnterScene>("BeforeEnterScene",
"entity",
sol::property(&BeforeEnterScene::entity, &BeforeEnterScene::set_entity),
"DebugString",
&BeforeEnterScene::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<AfterEnterScene>("AfterEnterScene",
"entity",
sol::property(&AfterEnterScene::entity, &AfterEnterScene::set_entity),
"DebugString",
&AfterEnterScene::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<BeforeLeaveScene>("BeforeLeaveScene",
"entity",
sol::property(&BeforeLeaveScene::entity, &BeforeLeaveScene::set_entity),
"DebugString",
&BeforeLeaveScene::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<AfterLeaveScene>("AfterLeaveScene",
"entity",
sol::property(&AfterLeaveScene::entity, &AfterLeaveScene::set_entity),
"DebugString",
&AfterLeaveScene::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<S2CEnterScene>("S2CEnterScene",
"entity",
sol::property(&S2CEnterScene::entity, &S2CEnterScene::set_entity),
"DebugString",
&S2CEnterScene::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

}
