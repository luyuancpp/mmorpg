#include "centre_kafka_command_router.h"

#include "proto/contracts/kafka/centre_command.pb.h"
#include "rpc/service_metadata/service_metadata.h"
#include "muduo/base/Logging.h"

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
