#include "logic/component/frame_comp.pb.h"
#include <sol/sol.hpp>
#include "thread_local/storage_lua.h"
void Pb2sol2frame_comp()
{
tls_lua_state.new_usertype<FrameTime>("FrameTime",
"previous_time",
sol::property(&FrameTime::previous_time, &FrameTime::set_previous_time),
"delta_time",
sol::property(&FrameTime::delta_time, &FrameTime::set_delta_time),
"target_fps",
sol::property(&FrameTime::target_fps, &FrameTime::set_target_fps),
"time_accumulator",
sol::property(&FrameTime::time_accumulator, &FrameTime::set_time_accumulator),
"current_frame",
sol::property(&FrameTime::current_frame, &FrameTime::set_current_frame),
"DebugString",
&FrameTime::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

}
