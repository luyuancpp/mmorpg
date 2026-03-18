#include "gate_kafka_command_router.h"

#include <messaging/kafka/kafka_proto_decoder.h>

#include "proto/contracts/kafka/gate_command.pb.h"
#include "proto/contracts/kafka/gate_event.pb.h"
#include "rpc/service_metadata/service_metadata.h"
#include "thread_context/dispatcher_manager.h"
#include "muduo/base/Logging.h"

namespace {

template <typename EventT>
void EnqueueGateKafkaEvent(const EventT& event)
{
    dispatcher.enqueue(event);
}

template <typename EventT>
bool DecodeAndEnqueueGateKafkaEvent(const std::string& topic, const std::string& payload)
{
    auto event = DecodeKafkaProtoPayload<EventT>(topic, payload);
    if (!event) {
        return false;
    }

    EnqueueGateKafkaEvent(*event);
    return true;
}

bool DecodeAndDispatchGateKafkaEventById(const std::string& topic, const contracts::kafka::GateCommand& command)
{
    if (!command.has_event_id()) {
        return false;
    }

    if (command.payload().empty()) {
        LOG_WARN << "GateCommand event payload is empty, topic=" << topic << ", event_id=" << command.event_id();
        return true;
    }

    auto eventMessage = ParseEventMessage(command.event_id(), command.payload());
    if (!eventMessage) {
        LOG_WARN << "GateCommand payload decode failed, topic=" << topic << ", event_id=" << command.event_id();
        return true;
    }

    DispatchProtoEvent(command.event_id(), *eventMessage);
    return true;
}

} // namespace

void DispatchGateKafkaCommand(const std::string& topic, const contracts::kafka::GateCommand& command)
{
    if (DecodeAndDispatchGateKafkaEventById(topic, command)) {
        return;
    }

    switch (command.command_type()) {
    case contracts::kafka::GateCommand::RoutePlayer: {
        if (!command.payload().empty()) {
            if (!DecodeAndEnqueueGateKafkaEvent<contracts::kafka::RoutePlayerEvent>(topic, command.payload())) {
                LOG_WARN << "RoutePlayer payload decode failed for session " << command.session_id();
            }
            return;
        }

        contracts::kafka::RoutePlayerEvent event;
        event.set_session_id(command.session_id());
        event.set_target_node_id(command.target_node_id());
        EnqueueGateKafkaEvent(event);
        return;
    }
    case contracts::kafka::GateCommand::KickPlayer: {
        if (!command.payload().empty()) {
            if (!DecodeAndEnqueueGateKafkaEvent<contracts::kafka::KickPlayerEvent>(topic, command.payload())) {
                LOG_WARN << "KickPlayer payload decode failed for session " << command.session_id();
            }
            return;
        }

        contracts::kafka::KickPlayerEvent event;
        event.set_session_id(command.session_id());
        EnqueueGateKafkaEvent(event);
        return;
    }
    case contracts::kafka::GateCommand::BindSession: {
        if (!command.payload().empty()) {
            if (!DecodeAndEnqueueGateKafkaEvent<contracts::kafka::BindSessionEvent>(topic, command.payload())) {
                LOG_WARN << "BindSession payload decode failed for session " << command.session_id();
            }
            return;
        }

        contracts::kafka::BindSessionEvent event;
        event.set_session_id(command.session_id());
        event.set_player_id(command.player_id());
        EnqueueGateKafkaEvent(event);
        return;
    }
    case contracts::kafka::GateCommand::LeaseExpired: {
        if (!command.payload().empty()) {
            if (!DecodeAndEnqueueGateKafkaEvent<contracts::kafka::PlayerLeaseExpiredEvent>(topic, command.payload())) {
                LOG_WARN << "LeaseExpired payload decode failed for session " << command.session_id();
            }
            return;
        }

        contracts::kafka::PlayerLeaseExpiredEvent event;
        event.set_player_id(command.player_id());
        event.set_session_id(command.session_id());
        EnqueueGateKafkaEvent(event);
        return;
    }
    default:
        LOG_WARN << "Unknown GateCommand type: " << command.command_type();
        return;
    }
}

