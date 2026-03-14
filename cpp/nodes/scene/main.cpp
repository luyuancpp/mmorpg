#include "muduo/net/EventLoop.h"

#include "node/system/node/simple_node.h"
#include "handler/rpc/scene_handler.h"
#include "table/code/all_table.h"
#include "core/config/config.h"
#include "handler/event/event_handler.h"
#include "world/world.h"
#include "threading/redis_manager.h"
#include "frame/manager/frame_time.h"
#include "time/comp/timer_task_comp.h"
#include "grpc/third_party/abseil-cpp/absl/log/initialize.h"

using namespace muduo;
using namespace muduo::net;

namespace {
void startSceneNode(EventLoop& loop)
{
    TimerTaskComp worldTimer;

    SimpleNode<SceneHandler> node(&loop, "logs/scene", SceneNodeService,
        CanConnectNodeTypeList{ CentreNodeService });
    tlsRedisSystem.Initialize();
    EventHandler::Register();
    World::InitializeSystemBeforeConnect();
    OnTablesLoadSuccess([] { ConfigSystem::OnConfigLoadSuccessful(); });

    node.SetAfterStart([&worldTimer](SimpleNode<SceneHandler>&) {
        World::ReadyForGame();
        worldTimer.RunEvery(tlsFrameTimeManager.frameTime.delta_time(), World::Update);
    });

    loop.loop();
}
}

int main(int argc, char* argv[])
{
    absl::InitializeLog();

    EventLoop loop;
    startSceneNode(loop);
    google::protobuf::ShutdownProtobufLibrary();
    return 0;
}