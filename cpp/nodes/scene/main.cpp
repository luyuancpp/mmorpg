#include "muduo/net/EventLoop.h"
#include "muduo/base/Logging.h"

#include "node/system/node/simple_node.h"
#include "node/system/node/node.h"
#include "node/system/node/node_entry.h"
#include "handler/rpc/scene_handler.h"
#include "table/code/all_table.h"
#include "core/config/config.h"
#include "handler/event/event_handler.h"
#include "handler/event/scene_kafka_command_router.h"
#include "world/world.h"
#include "core/system/redis.h"
#include "frame/manager/frame_time.h"
#include "node/system/node/node_kafka_command_handler.h"
#include "proto/contracts/kafka/scene_command.pb.h"

using namespace muduo;
using namespace muduo::net;

namespace {

struct SceneRuntimeContext {
    TimerTaskComp worldTimer;
    DependencyGate dependencyGate;
};

} // namespace

int main(int argc, char* argv[])
{
    return node::entry::RunSimpleNodeMainWithOwnedContext<SceneHandler, SceneRuntimeContext>(
        "logs/scene",
        SceneNodeService,
        Node::CanConnectNodeTypeList{ SceneManagerNodeService },
        [](EventLoop&, SceneRuntimeContext&) {},
        [](SimpleNode<SceneHandler>& node, SceneRuntimeContext& context) {
            tlsRedisSystem.Initialize();
            EventHandler::Register();
            World::InitializeSystemBeforeConnect();
            OnTablesLoadSuccess([] { ConfigSystem::OnConfigLoadSuccessful(); });

            node.SetAfterStart([&context](SimpleNode<SceneHandler>& n) {
                context.dependencyGate.WaitAndRun(n, { SceneManagerNodeService },
                    [&context](auto&) {
                        context.worldTimer.RunEvery(tlsFrameTimeManager.frameTime.delta_time(), World::Update);
                    }, "Scene");
            });

            // Kafka: unified registration path for all node command-consumers.
            node.SetKafkaHandlers([](SimpleNode<SceneHandler>& n) {
                node::kafka::KafkaCommandHandlerOptions options;
                options.topicPrefix = "scene";
                options.groupPrefix = "scene-group";
                options.nodeIdFieldNames = { "target_scene_id", "target_node_id" };
                options.instanceIdFieldNames = { "target_instance_id" };

                return node::kafka::RegisterKafkaCommandHandler<contracts::kafka::SceneCommand>(
                    n,
                    options,
                    [](const std::string& topic, const contracts::kafka::SceneCommand& command) {
                        DispatchSceneKafkaCommand(topic, command);
                    });
            });
        });
}