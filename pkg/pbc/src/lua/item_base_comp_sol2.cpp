#include "logic/component/item_base_comp.pb.h"
#include <sol/sol.hpp>
#include "thread_local/storage_lua.h"
void Pb2sol2item_base_comp()
{
tls_lua_state.new_usertype<ItemPBComponent>("ItemPBComponent",
"item_id",
sol::property(&ItemPBComponent::item_id, &ItemPBComponent::set_item_id),
"config_id",
sol::property(&ItemPBComponent::config_id, &ItemPBComponent::set_config_id),
"size",
sol::property(&ItemPBComponent::size, &ItemPBComponent::set_size),
"DebugString",
&ItemPBComponent::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

}
