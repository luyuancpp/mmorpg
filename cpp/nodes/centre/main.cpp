#include "muduo/net/EventLoop.h"

#include "node/system/node/simple_node.h"
#include "node/system/node/node_kafka_command_handler.h"
#include "handler/rpc/centre_service_handler.h"
#include "handler/event/event_handler.h"
#include "handler/event/centre_kafka_command_router.h"
#include "redis/system/redis.h"
#include "grpc/third_party/abseil-cpp/absl/log/initialize.h"
#include "proto/common/base/node.pb.h"
#include "proto/contracts/kafka/centre_command.pb.h"

using namespace muduo;
using namespace muduo::net;

namespace {
void startCentreNode(EventLoop& loop)
{
    SimpleNode<CentreHandler> node(&loop, "logs/centre", CentreNodeService);
    EventHandler::Register();
    tlsRedisSystem.Initialize();

    // Kafka: unified registration path for all node command-consumers.
    node.SetKafkaHandlers([](SimpleNode<CentreHandler>& n) {
        node::kafka::KafkaCommandHandlerOptions options;
        options.topicPrefix = "centre";
        options.groupPrefix = "centre-group";
        options.nodeIdFieldNames = { "target_centre_id", "target_node_id" };
        options.instanceIdFieldNames = { "target_instance_id" };

        return node::kafka::RegisterKafkaCommandHandler<contracts::kafka::CentreCommand>(
            n,
            options,
            [](const std::string& topic, const contracts::kafka::CentreCommand& command) {
                DispatchCentreKafkaCommand(topic, command);
            });
    });

    loop.loop();
}
}

int main(int argc, char* argv[])
{
    absl::InitializeLog();

    EventLoop loop;
    startCentreNode(loop);
    google::protobuf::ShutdownProtobufLibrary();
    return 0;
}