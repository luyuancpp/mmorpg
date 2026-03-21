#include "scene_kafka_command_router.h"

#include "proto/contracts/kafka/scene_command.pb.h"
#include "rpc/service_metadata/service_metadata.h"
#include "muduo/base/Logging.h"

void DispatchSceneKafkaCommand(const std::string& topic, const contracts::kafka::SceneCommand& command)
{
    if (!command.has_event_id()) {
        LOG_WARN << "SceneCommand missing event_id, topic=" << topic;
        return;
    }

    if (command.payload().empty()) {
        LOG_WARN << "SceneCommand payload is empty, topic=" << topic << ", event_id=" << command.event_id();
        return;
    }

    if (!DispatchProtoEvent(command.event_id(), command.payload())) {
        LOG_WARN << "SceneCommand payload decode failed, topic=" << topic << ", event_id=" << command.event_id();
    }
}
