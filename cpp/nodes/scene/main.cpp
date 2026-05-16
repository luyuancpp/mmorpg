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
#include "player/system/cross_zone_reaper.h"
#include "kafka/system/kafka.h"
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
            // Subscribe cross-zone migration topics.
            //
            // `player_migrate`     — destination side: receive players migrating
            //                        INTO this zone's scene nodes from elsewhere
            //                        (HandlePlayerMigration → InitPlayerFromAllData
            //                        → publish ACK).
            // `player_migrate_ack` — source side: receive ACKs confirming the
            //                        destination loaded the player so we can
            //                        clear PlayerFrozenComp + DestroyPlayer
            //                        (HandlePlayerMigrationAck).
            //
            // groupId is per-node-id so each scene node has its own consumer
            // group and consumes every relevant message — partition-key=playerId
            // ensures same-player ordering. If you put multiple nodes in the
            // same consumer group they'll round-robin partitions and miss ACKs
            // intended for their own outgoing migrations.
            //
            // See docs/design/cross-zone-readiness-audit.md §3 (Kafka self-
            // orchestrated design) and §10.3 option A (this wiring) for context.
            // Without this subscription the audit doc's "件 2/件 3" code paths
            // exist but never fire.
            const std::string crossZoneGroupId =
                "scene-cross-zone-" + std::to_string(n.GetNodeId());
            if (!n.RegisterKafkaMessageHandler(
                    {"player_migrate", "player_migrate_ack"},
                    crossZoneGroupId,
                    &KafkaSystem::KafkaMessageHandler))
            {
                LOG_ERROR << "Failed to subscribe cross-zone Kafka topics; "
                          << "cross-zone migration will not work on this node "
                          << "(group_id=" << crossZoneGroupId << ").";
            }

            // Start the cross-zone reaper. Without this the Frozen state
            // pattern (player_lifecycle.cpp) can leak — a player whose
            // `player_migrate_ack` is dropped or whose destination crashes
            // would stay frozen forever. The reaper scans the Redis
            // `player_migration:*` records every 10s, re-publishes
            // migrations whose ACK is overdue, and unfreezes / notifies
            // the client when retries are exhausted. It also performs a
            // restart-recovery sweep on the way in (StartTick calls
            // ScanAndRecover before arming the periodic timer) so source-
            // side crashes don't leak Redis records either.
            //
            // See docs/design/cross-zone-readiness-audit.md §3.2 件 3 + §7.
            CrossZoneReaper::StartTick(n.GetLoop());

            context->dependencyGate.WaitAndRun(n, { SceneManagerNodeService },
                [&context](auto&) {
                    context->worldTimer.RunEvery(tlsFrameTimeManager.frameTime.delta_time(), World::Update);
                }, "Scene");
        });

        loop.loop(); });
}