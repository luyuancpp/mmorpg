#include "logic/component/actor_comp.pb.h"
#include <sol/sol.hpp>
#include "thread_local/storage_lua.h"
void Pb2sol2actor_comp()
{
tls_lua_state.new_usertype<Location>("Location",
"x",
sol::property(&Location::x, &Location::set_x),
"y",
sol::property(&Location::y, &Location::set_y),
"z",
sol::property(&Location::z, &Location::set_z),
"DebugString",
&Location::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<Rotation>("Rotation",
"x",
sol::property(&Rotation::x, &Rotation::set_x),
"y",
sol::property(&Rotation::y, &Rotation::set_y),
"z",
sol::property(&Rotation::z, &Rotation::set_z),
"DebugString",
&Rotation::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<Scale>("Scale",
"x",
sol::property(&Scale::x, &Scale::set_x),
"y",
sol::property(&Scale::y, &Scale::set_y),
"z",
sol::property(&Scale::z, &Scale::set_z),
"DebugString",
&Scale::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<Transform>("Transform",
"location",
[](Transform& pb) ->decltype(auto){ return pb.location();},
"mutable_location",
[](Transform& pb) ->decltype(auto){ return pb.mutable_location();},
"rotation",
[](Transform& pb) ->decltype(auto){ return pb.rotation();},
"mutable_rotation",
[](Transform& pb) ->decltype(auto){ return pb.mutable_rotation();},
"scale",
[](Transform& pb) ->decltype(auto){ return pb.scale();},
"mutable_scale",
[](Transform& pb) ->decltype(auto){ return pb.mutable_scale();},
"DebugString",
&Transform::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<Velocity>("Velocity",
"x",
sol::property(&Velocity::x, &Velocity::set_x),
"y",
sol::property(&Velocity::y, &Velocity::set_y),
"z",
sol::property(&Velocity::z, &Velocity::set_z),
"DebugString",
&Velocity::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<Acceleration>("Acceleration",
"x",
sol::property(&Acceleration::x, &Acceleration::set_x),
"y",
sol::property(&Acceleration::y, &Acceleration::set_y),
"z",
sol::property(&Acceleration::z, &Acceleration::set_z),
"DebugString",
&Acceleration::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<ViewRadius>("ViewRadius",
"radius",
sol::property(&ViewRadius::radius, &ViewRadius::set_radius),
"DebugString",
&ViewRadius::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<BaseAttributesPBComponent>("BaseAttributesPBComponent",
"strength",
sol::property(&BaseAttributesPBComponent::strength, &BaseAttributesPBComponent::set_strength),
"stamina",
sol::property(&BaseAttributesPBComponent::stamina, &BaseAttributesPBComponent::set_stamina),
"health",
sol::property(&BaseAttributesPBComponent::health, &BaseAttributesPBComponent::set_health),
"mana",
sol::property(&BaseAttributesPBComponent::mana, &BaseAttributesPBComponent::set_mana),
"critchance",
sol::property(&BaseAttributesPBComponent::critchance, &BaseAttributesPBComponent::set_critchance),
"armor",
sol::property(&BaseAttributesPBComponent::armor, &BaseAttributesPBComponent::set_armor),
"resistance",
sol::property(&BaseAttributesPBComponent::resistance, &BaseAttributesPBComponent::set_resistance),
"DebugString",
&BaseAttributesPBComponent::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<LevelComponent>("LevelComponent",
"level",
sol::property(&LevelComponent::level, &LevelComponent::set_level),
"DebugString",
&LevelComponent::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

}
