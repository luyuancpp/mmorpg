#include "logic/server/game_scene.pb.h"
#include <sol/sol.hpp>
#include "thread_local/storage_lua.h"
void Pb2sol2game_scene()
{
tls_lua_state.new_usertype<GameSceneTest>("GameSceneTest",
"DebugString",
&GameSceneTest::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

}
