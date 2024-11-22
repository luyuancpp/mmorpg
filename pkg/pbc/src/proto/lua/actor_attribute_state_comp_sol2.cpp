#include "logic/component/actor_attribute_state_comp.pb.h"
#include <sol/sol.hpp>
#include "thread_local/storage_lua.h"
void Pb2sol2actor_attribute_state_comp()
{
tls_lua_state.new_usertype<CombatStateFlagsPbComponent>("CombatStateFlagsPbComponent",
"count_state_flags",
[](CombatStateFlagsPbComponent& pb, uint32_t key) ->decltype(auto){ return pb.state_flags().count(key);},
"insert_state_flags",
[](CombatStateFlagsPbComponent& pb, uint32_t key, bool value) ->decltype(auto){ return pb.mutable_state_flags()->emplace(key, value).second;},
"state_flags",
[](CombatStateFlagsPbComponent& pb, uint32_t key) ->decltype(auto){
 auto it =  pb.mutable_state_flags()->find(key);
 if (it == pb.mutable_state_flags()->end()){ return bool(); }
 return it->second;},
"state_flags_size",
&CombatStateFlagsPbComponent::state_flags_size,
"clear_state_flags",
&CombatStateFlagsPbComponent::clear_state_flags,
"DebugString",
&CombatStateFlagsPbComponent::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

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
