#include "muduo/net/EventLoop.h"
#include "muduo/base/Logging.h"

#include "node/system/node/node_entry.h"
#include "handler/rpc/scene_handler.h"
#include "core/config/config.h"
#include "world/world.h"
#include "core/system/redis.h"
#include "frame/manager/frame_time.h"
#include "proto/contracts/kafka/scene_command.pb.h"

using namespace muduo;
using namespace muduo::net;

namespace {

struct SceneRuntimeContext {
    TimerTaskComp worldTimer;
    DependencyGate dependencyGate;
};

struct SceneNodeHooks {
    struct TableLoadHandler { static void OnLoaded() { ConfigSystem::OnConfigLoadSuccessful(); } };
    using KafkaCommandType = contracts::kafka::SceneCommand;
};

} // namespace

int main(int argc, char* argv[])
{
    return node::entry::RunSimpleNodeMainWithOwnedContext<SceneHandler, SceneRuntimeContext, SceneNodeHooks>(
        SceneNodeService,
        Node::CanConnectNodeTypeList{ SceneManagerNodeService },
        [](Node& node, SceneRuntimeContext& context) {
            tlsRedisSystem.Initialize();
            World::InitializeSystemBeforeConnect();

            node.SetAfterStart([&context](Node& n) {
                context.dependencyGate.WaitAndRun(n, { SceneManagerNodeService },
                    [&context](auto&) {
                        context.worldTimer.RunEvery(tlsFrameTimeManager.frameTime.delta_time(), World::Update);
                    }, "Scene");
            });
        });
}