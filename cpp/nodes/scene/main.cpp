#include "muduo/net/EventLoop.h"
#include "muduo/base/Logging.h"

#include "node/system/node/node_entry.h"
#include "handler/rpc/scene_handler.h"
#include "handler/grpc/scene_node_service.h"
#include "core/config/config.h"
#include "world/world.h"
#include "core/system/redis.h"
#include "frame/manager/frame_time.h"
#include "player/system/player_lifecycle.h"
#include "proto/contracts/kafka/scene_command.pb.h"

using namespace muduo;
using namespace muduo::net;

namespace
{

    struct SceneRuntimeContext
    {
        TimerTaskComp worldTimer;
        DependencyGate dependencyGate;
        SceneNodeGrpcImpl grpcService;

        explicit SceneRuntimeContext(EventLoop& loop) : grpcService(loop) {}
    };

    struct SceneNodeHooks
    {
        struct TableLoadHandler
        {
            static void OnLoaded() { ConfigSystem::OnConfigLoadSuccessful(); }
        };
        using KafkaCommandType = contracts::kafka::SceneCommand;
    };

} // namespace

int main(int argc, char *argv[])
{
    return node::entry::RunNodeMain([](EventLoop &loop)
                                    {
        node::entry::detail::ApplyPreConstructionHooks<SceneNodeHooks>();

        auto context = std::make_unique<SceneRuntimeContext>(loop);

        SceneHandler handler;
        Node node(&loop, SceneNodeService,
                  Node::CanConnectNodeTypeList{ SceneManagerNodeService },
                  &handler);

        node::entry::detail::ApplyPostConstructionHooks<SceneNodeHooks>(node);

        node.RegisterGrpcService(&context->grpcService);

        node::entry::detail::InstallSignalHandlers(loop);

        tlsRedisSystem.Initialize(&loop);
        World::InitializeSystemBeforeConnect();

        // SIGTERM / conflict safety net: save all players via the full exit flow.
        // The preferred shutdown path is GmGracefulShutdown RPC, which also calls
        // HandleExitGameNode for each player before triggering Shutdown().
        auto exitAllPlayers = [](Node &)
        {
            auto view = tlsEcs.actorRegistry.view<Player>();
            LOG_INFO << "Emergency save: exiting " << view.size() << " online players before shutdown...";
            for (auto entity : view)
            {
                PlayerLifecycleSystem::HandleExitGameNode(entity);
            }
            LOG_INFO << "All players exited.";
        };
        node.SetBeforeShutdown(exitAllPlayers);
        node.SetOnConflictShutdown([exitAllPlayers](Node &n, NodeIdConflictReason)
                                   { exitAllPlayers(n); });

        node.SetAfterStart([&context](Node& n) {
            context->dependencyGate.WaitAndRun(n, { SceneManagerNodeService },
                [&context](auto&) {
                    context->worldTimer.RunEvery(tlsFrameTimeManager.frameTime.delta_time(), World::Update);
                }, "Scene");
        });

        loop.loop(); });
}