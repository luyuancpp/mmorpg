#include "logic/component/item_base_comp.pb.h"
#include <sol/sol.hpp>
#include "thread_local/storage_lua.h"
void Pb2sol2item_base_comp()
{
tls_lua_state.new_usertype<ItemPBComp>("ItemPBComp",
"item_id",
sol::property(&ItemPBComp::item_id, &ItemPBComp::set_item_id),
"config_id",
sol::property(&ItemPBComp::config_id, &ItemPBComp::set_config_id),
"size",
sol::property(&ItemPBComp::size, &ItemPBComp::set_size),
"DebugString",
&ItemPBComp::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

}
