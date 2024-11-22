#include "logic/component/actor_combat_state_comp.pb.h"
#include <sol/sol.hpp>
#include "thread_local/storage_lua.h"
void Pb2sol2actor_combat_state_comp()
{
tls_lua_state.new_usertype<CombatStatePbComponent>("CombatStatePbComponent",
"count_sources",
[](CombatStatePbComponent& pb, uint64_t key) ->decltype(auto){ return pb.sources().count(key);},
"insert_sources",
[](CombatStatePbComponent& pb, uint64_t key, bool value) ->decltype(auto){ return pb.mutable_sources()->emplace(key, value).second;},
"sources",
[](CombatStatePbComponent& pb, uint64_t key) ->decltype(auto){
 auto it =  pb.mutable_sources()->find(key);
 if (it == pb.mutable_sources()->end()){ return bool(); }
 return it->second;},
"sources_size",
&CombatStatePbComponent::sources_size,
"clear_sources",
&CombatStatePbComponent::clear_sources,
"DebugString",
&CombatStatePbComponent::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<CombatStateCollectionPbComponent>("CombatStateCollectionPbComponent",
"count_state_collection",
[](CombatStateCollectionPbComponent& pb, uint32_t key) ->decltype(auto){ return pb.state_collection().count(key);},
"insert_state_collection",
[](CombatStateCollectionPbComponent& pb, uint32_t key, CombatStatePbComponent& value) ->decltype(auto){ return pb.mutable_state_collection()->emplace(key, value).second;},
"state_collection",
[](CombatStateCollectionPbComponent& pb, uint32_t key) ->decltype(auto){
 auto it =  pb.mutable_state_collection()->find(key);
 if (it == pb.mutable_state_collection()->end()){ static CombatStatePbComponent instance; return instance; }
 return it->second;},
"state_collection_size",
&CombatStateCollectionPbComponent::state_collection_size,
"clear_state_collection",
&CombatStateCollectionPbComponent::clear_state_collection,
"DebugString",
&CombatStateCollectionPbComponent::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

}
