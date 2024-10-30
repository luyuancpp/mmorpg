#include "logic/component/actor_status_comp.pb.h"
#include <sol/sol.hpp>
#include "thread_local/storage_lua.h"
void Pb2sol2actor_status_comp()
{
tls_lua_state.new_usertype<CalculatedAttributesPbComponent>("CalculatedAttributesPbComponent",
"attack_power",
sol::property(&CalculatedAttributesPbComponent::attack_power, &CalculatedAttributesPbComponent::set_attack_power),
"defense_power",
sol::property(&CalculatedAttributesPbComponent::defense_power, &CalculatedAttributesPbComponent::set_defense_power),
"isDead",
sol::property(&CalculatedAttributesPbComponent::isDead, &CalculatedAttributesPbComponent::set_isDead),
"DebugString",
&CalculatedAttributesPbComponent::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<DerivedAttributesPbComponent>("DerivedAttributesPbComponent",
"max_health",
sol::property(&DerivedAttributesPbComponent::max_health, &DerivedAttributesPbComponent::set_max_health),
"DebugString",
&DerivedAttributesPbComponent::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

}
