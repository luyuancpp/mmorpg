#include "common/empty.pb.h"
#include <sol/sol.hpp>
#include "thread_local/storage_lua.h"
void Pb2sol2empty()
{
tls_lua_state.new_usertype<Empty>("Empty");
}
