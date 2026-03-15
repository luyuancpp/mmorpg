#include "muduo/net/EventLoop.h"

#include "node/system/node/simple_node.h"
#include "node/system/node/node_kafka_command_handler.h"
#include "handler/rpc/centre_service_handler.h"
#include "redis/system/redis.h"
#include "grpc/third_party/abseil-cpp/absl/log/initialize.h"
#include "proto/common/base/node.pb.h"
#include "proto/contracts/kafka/centre_command.pb.h"
#include "rpc/service_metadata/service_metadata.h"
#include "muduo/base/Logging.h"

void RegisterSceneEventHandlers();

using namespace muduo;
using namespace muduo::net;

namespace {
void DispatchCentreKafkaCommand(const std::string& topic, const contracts::kafka::CentreCommand& command)
{
    if (!command.has_event_id()) {
        LOG_WARN << "CentreCommand missing event_id, topic=" << topic;
        return;
    }

    if (command.payload().empty()) {
        LOG_WARN << "CentreCommand payload is empty, topic=" << topic << ", event_id=" << command.event_id();
        return;
    }

    auto eventMessage = ParseEventMessage(command.event_id(), command.payload());
    if (!eventMessage) {
        LOG_WARN << "CentreCommand payload decode failed, topic=" << topic << ", event_id=" << command.event_id();
        return;
    }

    DispatchProtoEvent(command.event_id(), *eventMessage);
}

void startCentreNode(EventLoop& loop)
{
    SimpleNode<CentreHandler> node(&loop, "logs/centre", CentreNodeService);
    RegisterSceneEventHandlers();
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