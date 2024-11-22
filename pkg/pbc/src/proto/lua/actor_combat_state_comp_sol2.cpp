#include "logic/component/actor_combat_state_comp.pb.h"
#include <sol/sol.hpp>
#include "thread_local/storage_lua.h"
void Pb2sol2actor_combat_state_comp()
{
tls_lua_state.new_usertype<CombatStateDetailsPbComponent>("CombatStateDetailsPbComponent",
"count_sources",
[](CombatStateDetailsPbComponent& pb, uint64_t key) ->decltype(auto){ return pb.sources().count(key);},
"insert_sources",
[](CombatStateDetailsPbComponent& pb, uint64_t key, bool value) ->decltype(auto){ return pb.mutable_sources()->emplace(key, value).second;},
"sources",
[](CombatStateDetailsPbComponent& pb, uint64_t key) ->decltype(auto){
 auto it =  pb.mutable_sources()->find(key);
 if (it == pb.mutable_sources()->end()){ return bool(); }
 return it->second;},
"sources_size",
&CombatStateDetailsPbComponent::sources_size,
"clear_sources",
&CombatStateDetailsPbComponent::clear_sources,
"DebugString",
&CombatStateDetailsPbComponent::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<CombatStateCollectionPbComponent>("CombatStateCollectionPbComponent",
"count_states",
[](CombatStateCollectionPbComponent& pb, uint32_t key) ->decltype(auto){ return pb.states().count(key);},
"insert_states",
[](CombatStateCollectionPbComponent& pb, uint32_t key, CombatStateDetailsPbComponent& value) ->decltype(auto){ return pb.mutable_states()->emplace(key, value).second;},
"states",
[](CombatStateCollectionPbComponent& pb, uint32_t key) ->decltype(auto){
 auto it =  pb.mutable_states()->find(key);
 if (it == pb.mutable_states()->end()){ static CombatStateDetailsPbComponent instance; return instance; }
 return it->second;},
"states_size",
&CombatStateCollectionPbComponent::states_size,
"clear_states",
&CombatStateCollectionPbComponent::clear_states,
"DebugString",
&CombatStateCollectionPbComponent::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

}
