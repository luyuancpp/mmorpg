#include "common/tip.pb.h"
#include <sol/sol.hpp>
#include "thread_local/storage_lua.h"
void Pb2sol2tip()
{
tls_lua_state.new_usertype<TipInfoMessage>("TipInfoMessage",
"id",
sol::property(&TipInfoMessage::id, &TipInfoMessage::set_id),
"add_parameters",
[](TipInfoMessage& pb, const std::string& value) ->decltype(auto){ return pb.add_parameters(value);},
"parameters",
[](TipInfoMessage& pb, int index) ->decltype(auto){ return pb.parameters(index);},
"set_parameters",
[](TipInfoMessage& pb, int index, const std::string& value) ->decltype(auto){ return pb.set_parameters(index, value);},
"parameters_size",
&TipInfoMessage::parameters_size,
"clear_parameters",
&TipInfoMessage::clear_parameters,
"DebugString",
&TipInfoMessage::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

}
