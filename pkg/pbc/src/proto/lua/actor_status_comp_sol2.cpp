#include "logic/component/actor_status_comp.pb.h"
#include <sol/sol.hpp>
#include "thread_local/storage_lua.h"
void Pb2sol2actor_status_comp()
{
tls_lua_state.new_usertype<CalculatedAttributesPBComponent>("CalculatedAttributesPBComponent",
"attack_power",
sol::property(&CalculatedAttributesPBComponent::attack_power, &CalculatedAttributesPBComponent::set_attack_power),
"defense_power",
sol::property(&CalculatedAttributesPBComponent::defense_power, &CalculatedAttributesPBComponent::set_defense_power),
"isDead",
sol::property(&CalculatedAttributesPBComponent::isDead, &CalculatedAttributesPBComponent::set_isDead),
"DebugString",
&CalculatedAttributesPBComponent::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<DerivedAttributesPBComponent>("DerivedAttributesPBComponent",
"max_health",
sol::property(&DerivedAttributesPBComponent::max_health, &DerivedAttributesPBComponent::set_max_health),
"DebugString",
&DerivedAttributesPBComponent::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

}
