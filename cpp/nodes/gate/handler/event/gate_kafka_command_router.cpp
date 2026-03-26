#include "gate_kafka_command_router.h"

#include <messaging/kafka/kafka_proto_decoder.h>

#include "proto/contracts/kafka/gate_command.pb.h"
#include "proto/contracts/kafka/gate_event.pb.h"
#include "rpc/service_metadata/contracts_kafka_gate_event_event_id.h"
#include "rpc/service_metadata/rpc_event_registry.h"
#include "thread_context/dispatcher_manager.h"
#include "muduo/base/Logging.h"

namespace
{

    bool DispatchGateEventFallback(const contracts::kafka::GateCommand &command)
    {
        switch (command.event_id())
        {
        case ContractsKafkaRoutePlayerEventEventId:
        {
            contracts::kafka::RoutePlayerEvent event;
            event.set_session_id(command.session_id());
            event.set_target_node_id(command.target_node_id());
            dispatcher.enqueue(event);
            return true;
        }
        case ContractsKafkaKickPlayerEventEventId:
        {
            contracts::kafka::KickPlayerEvent event;
            event.set_session_id(command.session_id());
            dispatcher.enqueue(event);
            return true;
        }
        case ContractsKafkaBindSessionEventEventId:
        {
            contracts::kafka::BindSessionEvent event;
            event.set_session_id(command.session_id());
            event.set_player_id(command.player_id());
            event.set_enter_gs_type(command.enter_gs_type());
            dispatcher.enqueue(event);
            return true;
        }
        case ContractsKafkaPlayerLeaseExpiredEventEventId:
        {
            contracts::kafka::PlayerLeaseExpiredEvent event;
            event.set_player_id(command.player_id());
            event.set_session_id(command.session_id());
            dispatcher.enqueue(event);
            return true;
        }
        default:
            return false;
        }
    }

} // namespace

void DispatchGateKafkaCommand(const std::string &topic, const contracts::kafka::GateCommand &command)
{
    if (command.event_id() == 0)
    {
        LOG_WARN << "GateCommand missing event_id, topic=" << topic;
        return;
    }

    if (command.payload().empty())
    {
        if (!DispatchGateEventFallback(command))
        {
            LOG_WARN << "GateCommand payload is empty and no fallback mapping, topic=" << topic
                     << ", event_id=" << command.event_id();
        }
        return;
    }

    if (!DispatchProtoEvent(command.event_id(), command.payload()))
    {
        LOG_WARN << "GateCommand payload decode failed, topic=" << topic << ", event_id=" << command.event_id();
    }
}
