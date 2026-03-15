#pragma once
#include <memory>
#include <string>
#include <array>
#include <functional>
#include <vector>
#include <unordered_set>
#include <google/protobuf/message.h>
#include <google/protobuf/service.h>
#include "entt/src/entt/entity/registry.hpp"

struct RpcService {
	const char* serviceName{nullptr};
	const char* methodName{nullptr};
	std::unique_ptr<::google::protobuf::Message> requestPrototype;
	std::unique_ptr<::google::protobuf::Message> responsePrototype;
	std::unique_ptr<::google::protobuf::Service> handlerInstance;
	uint32_t protocolType{0};
	uint32_t targetNodeType{0};
	std::function<void (
		entt::registry& registry,
		entt::entity nodeEntity,
		const google::protobuf::Message& message,
		const std::vector<std::string>& metaKeys,
		const std::vector<std::string>& metaValues
	)> messageSender;
};

struct ProtoEvent {
	const char* eventName{nullptr};
	std::unique_ptr<::google::protobuf::Message> prototype;
	void (*dispatcher)(const ::google::protobuf::Message& message){nullptr};
};

constexpr uint32_t kMaxMessageLen = 94;
constexpr uint32_t kMaxEventLen = 36;
constexpr uint32_t AcceptMissionEventEventId = 0;
constexpr uint32_t AfterEnterSceneEventId = 1;
constexpr uint32_t AfterLeaveSceneEventId = 2;
constexpr uint32_t BeKillEventEventId = 3;
constexpr uint32_t BeforeEnterSceneEventId = 4;
constexpr uint32_t BeforeLeaveSceneEventId = 5;
constexpr uint32_t BuffTestEvetEventId = 6;
constexpr uint32_t CombatStateAddedPbEventEventId = 7;
constexpr uint32_t CombatStateRemovedPbEventEventId = 8;
constexpr uint32_t ConnectToNodePbEventEventId = 9;
constexpr uint32_t ContractsKafkaBindSessionEventEventId = 32;
constexpr uint32_t ContractsKafkaKickPlayerEventEventId = 10;
constexpr uint32_t ContractsKafkaPlayerDisconnectedEventEventId = 33;
constexpr uint32_t ContractsKafkaPlayerLeaseExpiredEventEventId = 34;
constexpr uint32_t ContractsKafkaPlayerLifecycleCommandEventId = 35;
constexpr uint32_t ContractsKafkaRoutePlayerEventEventId = 11;
constexpr uint32_t InitializeActorComponentsEventEventId = 12;
constexpr uint32_t InitializeNpcComponentsEventEventId = 13;
constexpr uint32_t InitializePlayerComponentsEventEventId = 14;
constexpr uint32_t InterruptCurrentStatePbEventEventId = 15;
constexpr uint32_t MissionConditionEventEventId = 16;
constexpr uint32_t OnAcceptedMissionEventEventId = 17;
constexpr uint32_t OnConnect2CentrePbEventEventId = 18;
constexpr uint32_t OnConnect2LoginEventId = 19;
constexpr uint32_t OnMissionAwardEventEventId = 20;
constexpr uint32_t OnNodeAddPbEventEventId = 21;
constexpr uint32_t OnNodeConnectedPbEventEventId = 22;
constexpr uint32_t OnNodeRemovePbEventEventId = 23;
constexpr uint32_t OnSceneCreatedEventId = 24;
constexpr uint32_t OnSceneDestroyedEventId = 25;
constexpr uint32_t OnServerStartEventId = 26;
constexpr uint32_t PlayerMigrationPbEventEventId = 27;
constexpr uint32_t PlayerUpgradeEventEventId = 28;
constexpr uint32_t RegisterPlayerEventEventId = 29;
constexpr uint32_t S2CEnterSceneEventId = 30;
constexpr uint32_t SkillExecutedEventEventId = 31;

extern std::array<RpcService, kMaxMessageLen> gRpcServiceRegistry;
extern std::array<ProtoEvent, kMaxEventLen> gProtoEventRegistry;
extern std::unordered_set<uint32_t> gClientMessageIdWhitelist;

using MessageUniquePtr = std::unique_ptr<google::protobuf::Message>;

void InitMessageInfo();
void InitEventInfo();
bool IsValidEventId(uint32_t eventId);
MessageUniquePtr NewEventMessage(uint32_t eventId);
MessageUniquePtr ParseEventMessage(uint32_t eventId, const std::string& payload);
void DispatchProtoEvent(uint32_t eventId, const google::protobuf::Message& message);
