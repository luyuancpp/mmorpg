#pragma once

#include <algorithm>
#include <string>
#include <vector>

#include <google/protobuf/descriptor.h>
#include <google/protobuf/message.h>

#include "muduo/base/Logging.h"
#include "messaging/kafka/kafka_proto_decoder.h"
#include "node/system/node/node.h"
#include "rpc/service_metadata/rpc_event_registry.h"
#include <node_config_manager.h>

namespace node::kafka {

struct KafkaCommandHandlerOptions {
    std::string topicPrefix;
    std::string groupPrefix;
    std::vector<std::string> nodeIdFieldNames{ "target_node_id" };
    std::vector<std::string> instanceIdFieldNames{ "target_instance_id" };
};

// Derive KafkaCommandHandlerOptions from node type.
// e.g. GateNodeService -> topicPrefix="gate", groupPrefix="gate-group",
//      nodeIdFieldNames={"target_gate_id","target_node_id"}.
inline KafkaCommandHandlerOptions BuildDefaultKafkaOptions(uint32_t nodeType)
{
    const std::string name = NodeUtils::NodeTypeToShortName(nodeType);

    KafkaCommandHandlerOptions options;
    options.topicPrefix = name;
    options.groupPrefix = name + "-group";
    options.nodeIdFieldNames = { "target_" + name + "_id", "target_node_id" };
    options.instanceIdFieldNames = { "target_instance_id" };
    return options;
}

namespace detail {

inline bool TryReadUintField(const google::protobuf::Message& msg,
    const std::vector<std::string>& candidateFields,
    uint64_t& outValue,
    std::string& outFieldName)
{
    const auto* descriptor = msg.GetDescriptor();
    const auto* reflection = msg.GetReflection();
    if (!descriptor || !reflection) {
        return false;
    }

    for (const auto& fieldName : candidateFields) {
        const auto* field = descriptor->FindFieldByName(fieldName);
        if (!field) {
            continue;
        }
        if (field->is_repeated()) {
            continue;
        }

        switch (field->cpp_type()) {
        case google::protobuf::FieldDescriptor::CPPTYPE_UINT32:
            outValue = reflection->GetUInt32(msg, field);
            outFieldName = fieldName;
            return true;
        case google::protobuf::FieldDescriptor::CPPTYPE_UINT64:
            outValue = reflection->GetUInt64(msg, field);
            outFieldName = fieldName;
            return true;
        default:
            continue;
        }
    }

    return false;
}

inline bool TryReadStringField(const google::protobuf::Message& msg,
    const std::vector<std::string>& candidateFields,
    std::string& outValue,
    std::string& outFieldName)
{
    const auto* descriptor = msg.GetDescriptor();
    const auto* reflection = msg.GetReflection();
    if (!descriptor || !reflection) {
        return false;
    }

    for (const auto& fieldName : candidateFields) {
        const auto* field = descriptor->FindFieldByName(fieldName);
        if (!field) {
            continue;
        }
        if (field->is_repeated()) {
            continue;
        }
        if (field->cpp_type() != google::protobuf::FieldDescriptor::CPPTYPE_STRING) {
            continue;
        }

        outValue = reflection->GetString(msg, field);
        outFieldName = fieldName;
        return true;
    }

    return false;
}

template <typename CommandT>
bool ValidateCommandTarget(const std::string& topic,
    const CommandT& command,
    uint32_t nodeId,
    const std::string& nodeInstanceId,
    const KafkaCommandHandlerOptions& options)
{
    uint64_t targetNodeId = 0;
    std::string nodeIdFieldName;
    if (TryReadUintField(command, options.nodeIdFieldNames, targetNodeId, nodeIdFieldName)) {
        if (targetNodeId != 0 && targetNodeId != nodeId) {
            LOG_WARN << "Kafka command node target mismatch ignored. field=" << nodeIdFieldName
                << ", target=" << targetNodeId << ", current=" << nodeId << ", topic=" << topic;
            return false;
        }
    }

    std::string targetInstanceId;
    std::string instanceFieldName;
    if (TryReadStringField(command, options.instanceIdFieldNames, targetInstanceId, instanceFieldName)) {
        if (!targetInstanceId.empty() && targetInstanceId != nodeInstanceId) {
            LOG_WARN << "Kafka command stale instance ignored. field=" << instanceFieldName
                << ", target=" << targetInstanceId << ", current=" << nodeInstanceId << ", topic=" << topic;
            return false;
        }
    }

    return true;
}

} // namespace detail

template <typename CommandT, typename DispatchFn>
bool RegisterKafkaCommandHandler(Node& node,
    const KafkaCommandHandlerOptions& options,
    DispatchFn&& dispatchFn)
{
    const std::string topic = options.topicPrefix + "-" + std::to_string(node.GetNodeId());

    auto& kafkaConfig = tlsNodeConfigManager.GetBaseDeployConfig().kafka();
    std::string groupId = kafkaConfig.group_id();
    if (groupId.empty()) {
        groupId = options.groupPrefix + "-" + std::to_string(node.GetNodeId());
    }

    LOG_INFO << "Registering Kafka command handler. topic=" << topic << ", group_id=" << groupId;

    return node.RegisterKafkaMessageHandler({ topic }, groupId,
        [&node, dispatch = std::forward<DispatchFn>(dispatchFn), options](const std::string& t, const std::string& payload) {
            auto command = DecodeKafkaProtoPayload<CommandT>(t, payload);
            if (!command) {
                return;
            }

            if (!detail::ValidateCommandTarget(t, *command, node.GetNodeId(), node.GetNodeInfo().node_uuid(), options)) {
                return;
            }

            dispatch(t, *command);
        });
}

// Convenience overload: auto-derive options from node type.
template <typename CommandT, typename DispatchFn>
bool RegisterKafkaCommandHandler(Node& node,
    DispatchFn&& dispatchFn)
{
    return RegisterKafkaCommandHandler<CommandT>(
        node, BuildDefaultKafkaOptions(node.GetNodeType()),
        std::forward<DispatchFn>(dispatchFn));
}

// Default dispatch: validate event_id + payload, then DispatchProtoEvent.
// Works for any command proto with event_id() and payload() accessors.
template <typename CommandT>
void DefaultKafkaCommandDispatch(const std::string& topic, const CommandT& command)
{
    if (command.event_id() == 0) {
        LOG_WARN << "Kafka command missing event_id, topic=" << topic;
        return;
    }
    if (command.payload().empty()) {
        LOG_WARN << "Kafka command payload is empty, topic=" << topic
                 << ", event_id=" << command.event_id();
        return;
    }
    if (!DispatchProtoEvent(command.event_id(), command.payload())) {
        LOG_WARN << "Kafka command dispatch failed, topic=" << topic
                 << ", event_id=" << command.event_id();
    }
}

// Fully-automatic overload: auto-derive options + use default dispatch.
template <typename CommandT>
bool RegisterKafkaCommandHandler(Node& node)
{
    return RegisterKafkaCommandHandler<CommandT>(
        node, BuildDefaultKafkaOptions(node.GetNodeType()),
        DefaultKafkaCommandDispatch<CommandT>);
}

} // namespace node::kafka
