#include <array>
#include "service_metadata.h"
#include "proto/common/base/node.pb.h"
#include "threading/dispatcher_manager.h"

#include "proto/centre/centre_player.pb.h"
#include "proto/centre/centre_player_scene.pb.h"
#include "proto/centre/centre_scene.pb.h"
#include "proto/centre/centre_service.pb.h"
#include "proto/data_service/data_service.grpc.pb.h"
#include "proto/etcd/etcd.grpc.pb.h"
#include "proto/gate/gate_service.pb.h"
#include "proto/login/login.grpc.pb.h"
#include "proto/scene/game_client_player.pb.h"
#include "proto/scene/game_player.pb.h"
#include "proto/scene/game_player_scene.pb.h"
#include "proto/scene/game_scene.pb.h"
#include "proto/scene/player_scene.pb.h"
#include "proto/scene/player_skill.pb.h"
#include "proto/scene/player_state_attribute_sync.pb.h"
#include "proto/scene/scene.pb.h"
#include "proto/scene_manager/scene_manager_service.grpc.pb.h"

#include "rpc/service_metadata/centre_player_service_metadata.h"
#include "rpc/service_metadata/centre_player_scene_service_metadata.h"
#include "rpc/service_metadata/centre_scene_service_metadata.h"
#include "rpc/service_metadata/centre_service_service_metadata.h"
#include "rpc/service_metadata/data_service_service_metadata.h"
#include "rpc/service_metadata/etcd_service_metadata.h"
#include "rpc/service_metadata/gate_service_service_metadata.h"
#include "rpc/service_metadata/login_service_metadata.h"
#include "rpc/service_metadata/game_client_player_service_metadata.h"
#include "rpc/service_metadata/game_player_service_metadata.h"
#include "rpc/service_metadata/game_player_scene_service_metadata.h"
#include "rpc/service_metadata/game_scene_service_metadata.h"
#include "rpc/service_metadata/player_scene_service_metadata.h"
#include "rpc/service_metadata/player_skill_service_metadata.h"
#include "rpc/service_metadata/player_state_attribute_sync_service_metadata.h"
#include "rpc/service_metadata/scene_service_metadata.h"
#include "rpc/service_metadata/scene_manager_service_service_metadata.h"

#include "proto/common/event/mission_event.pb.h"
#include "proto/common/event/scene_event.pb.h"
#include "proto/common/event/combat_event.pb.h"
#include "proto/common/event/buff_event.pb.h"
#include "proto/common/event/actor_combat_state_event.pb.h"
#include "proto/common/event/node_event.pb.h"
#include "proto/contracts/kafka/gate_event.pb.h"
#include "proto/common/event/actor_event.pb.h"
#include "proto/common/event/npc_event.pb.h"
#include "proto/common/event/player_event.pb.h"
#include "proto/common/event/server_event.pb.h"
#include "proto/common/event/player_migration_event.pb.h"
#include "proto/common/event/skill_event.pb.h"

class CentrePlayerUtilityImpl final : public CentrePlayerUtility {};
class CentrePlayerSceneImpl final : public CentrePlayerScene {};
class CentreSceneImpl final : public CentreScene {};
class CentreImpl final : public Centre {};
class GateImpl final : public Gate {};
class SceneClientPlayerCommonImpl final : public SceneClientPlayerCommon {};
class ScenePlayerImpl final : public ScenePlayer {};
class SceneScenePlayerImpl final : public SceneScenePlayer {};
class SceneSceneImpl final : public SceneScene {};
class SceneSceneClientPlayerImpl final : public SceneSceneClientPlayer {};
class SceneSkillClientPlayerImpl final : public SceneSkillClientPlayer {};
class ScenePlayerSyncImpl final : public ScenePlayerSync {};
class SceneImpl final : public Scene {};
namespace {

void DispatchAcceptMissionEvent(const google::protobuf::Message& message)
{
    dispatcher.enqueue(static_cast<const AcceptMissionEvent&>(message));
}

void DispatchAfterEnterScene(const google::protobuf::Message& message)
{
    dispatcher.enqueue(static_cast<const AfterEnterScene&>(message));
}

void DispatchAfterLeaveScene(const google::protobuf::Message& message)
{
    dispatcher.enqueue(static_cast<const AfterLeaveScene&>(message));
}

void DispatchBeKillEvent(const google::protobuf::Message& message)
{
    dispatcher.enqueue(static_cast<const BeKillEvent&>(message));
}

void DispatchBeforeEnterScene(const google::protobuf::Message& message)
{
    dispatcher.enqueue(static_cast<const BeforeEnterScene&>(message));
}

void DispatchBeforeLeaveScene(const google::protobuf::Message& message)
{
    dispatcher.enqueue(static_cast<const BeforeLeaveScene&>(message));
}

void DispatchBuffTestEvet(const google::protobuf::Message& message)
{
    dispatcher.enqueue(static_cast<const BuffTestEvet&>(message));
}

void DispatchCombatStateAddedPbEvent(const google::protobuf::Message& message)
{
    dispatcher.enqueue(static_cast<const CombatStateAddedPbEvent&>(message));
}

void DispatchCombatStateRemovedPbEvent(const google::protobuf::Message& message)
{
    dispatcher.enqueue(static_cast<const CombatStateRemovedPbEvent&>(message));
}

void DispatchConnectToNodePbEvent(const google::protobuf::Message& message)
{
    dispatcher.enqueue(static_cast<const ConnectToNodePbEvent&>(message));
}

void DispatchContractsKafkaKickPlayerEvent(const google::protobuf::Message& message)
{
    dispatcher.enqueue(static_cast<const contracts::kafka::KickPlayerEvent&>(message));
}

void DispatchContractsKafkaRoutePlayerEvent(const google::protobuf::Message& message)
{
    dispatcher.enqueue(static_cast<const contracts::kafka::RoutePlayerEvent&>(message));
}

void DispatchInitializeActorComponentsEvent(const google::protobuf::Message& message)
{
    dispatcher.enqueue(static_cast<const InitializeActorComponentsEvent&>(message));
}

void DispatchInitializeNpcComponentsEvent(const google::protobuf::Message& message)
{
    dispatcher.enqueue(static_cast<const InitializeNpcComponentsEvent&>(message));
}

void DispatchInitializePlayerComponentsEvent(const google::protobuf::Message& message)
{
    dispatcher.enqueue(static_cast<const InitializePlayerComponentsEvent&>(message));
}

void DispatchInterruptCurrentStatePbEvent(const google::protobuf::Message& message)
{
    dispatcher.enqueue(static_cast<const InterruptCurrentStatePbEvent&>(message));
}

void DispatchMissionConditionEvent(const google::protobuf::Message& message)
{
    dispatcher.enqueue(static_cast<const MissionConditionEvent&>(message));
}

void DispatchOnAcceptedMissionEvent(const google::protobuf::Message& message)
{
    dispatcher.enqueue(static_cast<const OnAcceptedMissionEvent&>(message));
}

void DispatchOnConnect2CentrePbEvent(const google::protobuf::Message& message)
{
    dispatcher.enqueue(static_cast<const OnConnect2CentrePbEvent&>(message));
}

void DispatchOnConnect2Login(const google::protobuf::Message& message)
{
    dispatcher.enqueue(static_cast<const OnConnect2Login&>(message));
}

void DispatchOnMissionAwardEvent(const google::protobuf::Message& message)
{
    dispatcher.enqueue(static_cast<const OnMissionAwardEvent&>(message));
}

void DispatchOnNodeAddPbEvent(const google::protobuf::Message& message)
{
    dispatcher.enqueue(static_cast<const OnNodeAddPbEvent&>(message));
}

void DispatchOnNodeConnectedPbEvent(const google::protobuf::Message& message)
{
    dispatcher.enqueue(static_cast<const OnNodeConnectedPbEvent&>(message));
}

void DispatchOnNodeRemovePbEvent(const google::protobuf::Message& message)
{
    dispatcher.enqueue(static_cast<const OnNodeRemovePbEvent&>(message));
}

void DispatchOnSceneCreated(const google::protobuf::Message& message)
{
    dispatcher.enqueue(static_cast<const OnSceneCreated&>(message));
}

void DispatchOnSceneDestroyed(const google::protobuf::Message& message)
{
    dispatcher.enqueue(static_cast<const OnSceneDestroyed&>(message));
}

void DispatchOnServerStart(const google::protobuf::Message& message)
{
    dispatcher.enqueue(static_cast<const OnServerStart&>(message));
}

void DispatchPlayerMigrationPbEvent(const google::protobuf::Message& message)
{
    dispatcher.enqueue(static_cast<const PlayerMigrationPbEvent&>(message));
}

void DispatchPlayerUpgradeEvent(const google::protobuf::Message& message)
{
    dispatcher.enqueue(static_cast<const PlayerUpgradeEvent&>(message));
}

void DispatchRegisterPlayerEvent(const google::protobuf::Message& message)
{
    dispatcher.enqueue(static_cast<const RegisterPlayerEvent&>(message));
}

void DispatchS2CEnterScene(const google::protobuf::Message& message)
{
    dispatcher.enqueue(static_cast<const S2CEnterScene&>(message));
}

void DispatchSkillExecutedEvent(const google::protobuf::Message& message)
{
    dispatcher.enqueue(static_cast<const SkillExecutedEvent&>(message));
}

} // namespace

namespace data_service{void SendDataServiceLoadPlayerData(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace data_service{void SendDataServiceSavePlayerData(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace data_service{void SendDataServiceGetPlayerField(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace data_service{void SendDataServiceSetPlayerField(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace data_service{void SendDataServiceRegisterPlayerZone(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace data_service{void SendDataServiceGetPlayerHomeZone(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace data_service{void SendDataServiceBatchGetPlayerHomeZone(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace data_service{void SendDataServiceDeletePlayerData(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace etcdserverpb{void SendKVRange(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace etcdserverpb{void SendKVPut(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace etcdserverpb{void SendKVDeleteRange(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace etcdserverpb{void SendKVTxn(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace etcdserverpb{void SendKVCompact(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace etcdserverpb{void SendWatchWatch(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace etcdserverpb{void SendLeaseLeaseGrant(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace etcdserverpb{void SendLeaseLeaseRevoke(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace etcdserverpb{void SendLeaseLeaseKeepAlive(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace etcdserverpb{void SendLeaseLeaseTimeToLive(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace etcdserverpb{void SendLeaseLeaseLeases(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace loginpb{void SendClientPlayerLoginLogin(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace loginpb{void SendClientPlayerLoginCreatePlayer(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace loginpb{void SendClientPlayerLoginEnterGame(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace loginpb{void SendClientPlayerLoginLeaveGame(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace loginpb{void SendClientPlayerLoginDisconnect(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace scene_manager{void SendSceneManagerCreateScene(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace scene_manager{void SendSceneManagerDestroyScene(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace scene_manager{void SendSceneManagerEnterSceneByCentre(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace scene_manager{void SendSceneManagerLeaveSceneByCentre(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}

std::unordered_set<uint32_t> gClientMessageIdWhitelist;
std::array<RpcService, 94> gRpcServiceRegistry;
std::array<ProtoEvent, 32> gProtoEventRegistry;

void InitMessageInfo()
{
    gRpcServiceRegistry[CentrePlayerUtilitySendTipToClientMessageId] = RpcService{"CentrePlayerUtility", "SendTipToClient", std::make_unique_for_overwrite<::TipInfoMessage>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<CentrePlayerUtilityImpl>(), 0, eNodeType::CentreNodeService};
    gRpcServiceRegistry[CentrePlayerUtilityKickPlayerMessageId] = RpcService{"CentrePlayerUtility", "KickPlayer", std::make_unique_for_overwrite<::CentreKickPlayerRequest>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<CentrePlayerUtilityImpl>(), 0, eNodeType::CentreNodeService};
    gRpcServiceRegistry[CentrePlayerSceneEnterSceneMessageId] = RpcService{"CentrePlayerScene", "EnterScene", std::make_unique_for_overwrite<::CentreEnterSceneRequest>(), std::make_unique_for_overwrite<::google::protobuf::Empty>(), std::make_unique_for_overwrite<CentrePlayerSceneImpl>(), 0, eNodeType::CentreNodeService};
    gRpcServiceRegistry[CentrePlayerSceneLeaveSceneMessageId] = RpcService{"CentrePlayerScene", "LeaveScene", std::make_unique_for_overwrite<::CentreLeaveSceneRequest>(), std::make_unique_for_overwrite<::google::protobuf::Empty>(), std::make_unique_for_overwrite<CentrePlayerSceneImpl>(), 0, eNodeType::CentreNodeService};
    gRpcServiceRegistry[CentrePlayerSceneLeaveSceneAsyncSavePlayerCompleteMessageId] = RpcService{"CentrePlayerScene", "LeaveSceneAsyncSavePlayerComplete", std::make_unique_for_overwrite<::CentreLeaveSceneAsyncSavePlayerCompleteRequest>(), std::make_unique_for_overwrite<::google::protobuf::Empty>(), std::make_unique_for_overwrite<CentrePlayerSceneImpl>(), 0, eNodeType::CentreNodeService};
    gRpcServiceRegistry[CentrePlayerSceneSceneInfoC2SMessageId] = RpcService{"CentrePlayerScene", "SceneInfoC2S", std::make_unique_for_overwrite<::CentreSceneInfoRequest>(), std::make_unique_for_overwrite<::google::protobuf::Empty>(), std::make_unique_for_overwrite<CentrePlayerSceneImpl>(), 0, eNodeType::CentreNodeService};
    gRpcServiceRegistry[CentreSceneRegisterSceneMessageId] = RpcService{"CentreScene", "RegisterScene", std::make_unique_for_overwrite<::RegisterSceneRequest>(), std::make_unique_for_overwrite<::RegisterSceneResponse>(), std::make_unique_for_overwrite<CentreSceneImpl>(), 0, eNodeType::CentreNodeService};
    gRpcServiceRegistry[CentreSceneUnRegisterSceneMessageId] = RpcService{"CentreScene", "UnRegisterScene", std::make_unique_for_overwrite<::UnRegisterSceneRequest>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<CentreSceneImpl>(), 0, eNodeType::CentreNodeService};
    gRpcServiceRegistry[CentreGatePlayerServiceMessageId] = RpcService{"Centre", "GatePlayerService", std::make_unique_for_overwrite<::GateClientMessageRequest>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<CentreImpl>(), 0, eNodeType::CentreNodeService};
    gRpcServiceRegistry[CentreGateSessionDisconnectMessageId] = RpcService{"Centre", "GateSessionDisconnect", std::make_unique_for_overwrite<::GateSessionDisconnectRequest>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<CentreImpl>(), 0, eNodeType::CentreNodeService};
    gRpcServiceRegistry[CentreLoginNodeAccountLoginMessageId] = RpcService{"Centre", "LoginNodeAccountLogin", std::make_unique_for_overwrite<::CentreLoginRequest>(), std::make_unique_for_overwrite<::CentreLoginResponse>(), std::make_unique_for_overwrite<CentreImpl>(), 0, eNodeType::CentreNodeService};
    gRpcServiceRegistry[CentreLoginNodeEnterGameMessageId] = RpcService{"Centre", "LoginNodeEnterGame", std::make_unique_for_overwrite<::CentrePlayerGameNodeEntryRequest>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<CentreImpl>(), 0, eNodeType::CentreNodeService};
    gRpcServiceRegistry[CentreLoginNodeLeaveGameMessageId] = RpcService{"Centre", "LoginNodeLeaveGame", std::make_unique_for_overwrite<::LoginNodeLeaveGameRequest>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<CentreImpl>(), 0, eNodeType::CentreNodeService};
    gRpcServiceRegistry[CentreLoginNodeSessionDisconnectMessageId] = RpcService{"Centre", "LoginNodeSessionDisconnect", std::make_unique_for_overwrite<::GateSessionDisconnectRequest>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<CentreImpl>(), 0, eNodeType::CentreNodeService};
    gRpcServiceRegistry[CentrePlayerServiceMessageId] = RpcService{"Centre", "PlayerService", std::make_unique_for_overwrite<::NodeRouteMessageRequest>(), std::make_unique_for_overwrite<::NodeRouteMessageResponse>(), std::make_unique_for_overwrite<CentreImpl>(), 0, eNodeType::CentreNodeService};
    gRpcServiceRegistry[CentreEnterGsSucceedMessageId] = RpcService{"Centre", "EnterGsSucceed", std::make_unique_for_overwrite<::EnterGameNodeSuccessRequest>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<CentreImpl>(), 0, eNodeType::CentreNodeService};
    gRpcServiceRegistry[CentreRouteNodeStringMsgMessageId] = RpcService{"Centre", "RouteNodeStringMsg", std::make_unique_for_overwrite<::RouteMessageRequest>(), std::make_unique_for_overwrite<::RouteMessageResponse>(), std::make_unique_for_overwrite<CentreImpl>(), 0, eNodeType::CentreNodeService};
    gRpcServiceRegistry[CentreRoutePlayerStringMsgMessageId] = RpcService{"Centre", "RoutePlayerStringMsg", std::make_unique_for_overwrite<::RoutePlayerMessageRequest>(), std::make_unique_for_overwrite<::RoutePlayerMessageResponse>(), std::make_unique_for_overwrite<CentreImpl>(), 0, eNodeType::CentreNodeService};
    gRpcServiceRegistry[CentreInitSceneNodeMessageId] = RpcService{"Centre", "InitSceneNode", std::make_unique_for_overwrite<::InitSceneNodeRequest>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<CentreImpl>(), 0, eNodeType::CentreNodeService};
    gRpcServiceRegistry[CentreNodeHandshakeMessageId] = RpcService{"Centre", "NodeHandshake", std::make_unique_for_overwrite<::NodeHandshakeRequest>(), std::make_unique_for_overwrite<::NodeHandshakeResponse>(), std::make_unique_for_overwrite<CentreImpl>(), 0, eNodeType::CentreNodeService};
    gRpcServiceRegistry[DataServiceLoadPlayerDataMessageId] = RpcService{"DataService", "LoadPlayerData", std::make_unique_for_overwrite<::data_service::LoadPlayerDataRequest>(), std::make_unique_for_overwrite<::data_service::LoadPlayerDataResponse>(), nullptr, 0, eNodeType::DataServiceNodeService, data_service::SendDataServiceLoadPlayerData};
    gRpcServiceRegistry[DataServiceSavePlayerDataMessageId] = RpcService{"DataService", "SavePlayerData", std::make_unique_for_overwrite<::data_service::SavePlayerDataRequest>(), std::make_unique_for_overwrite<::data_service::SavePlayerDataResponse>(), nullptr, 0, eNodeType::DataServiceNodeService, data_service::SendDataServiceSavePlayerData};
    gRpcServiceRegistry[DataServiceGetPlayerFieldMessageId] = RpcService{"DataService", "GetPlayerField", std::make_unique_for_overwrite<::data_service::GetPlayerFieldRequest>(), std::make_unique_for_overwrite<::data_service::GetPlayerFieldResponse>(), nullptr, 0, eNodeType::DataServiceNodeService, data_service::SendDataServiceGetPlayerField};
    gRpcServiceRegistry[DataServiceSetPlayerFieldMessageId] = RpcService{"DataService", "SetPlayerField", std::make_unique_for_overwrite<::data_service::SetPlayerFieldRequest>(), std::make_unique_for_overwrite<::data_service::SetPlayerFieldResponse>(), nullptr, 0, eNodeType::DataServiceNodeService, data_service::SendDataServiceSetPlayerField};
    gRpcServiceRegistry[DataServiceRegisterPlayerZoneMessageId] = RpcService{"DataService", "RegisterPlayerZone", std::make_unique_for_overwrite<::data_service::RegisterPlayerZoneRequest>(), std::make_unique_for_overwrite<::google::protobuf::Empty>(), nullptr, 0, eNodeType::DataServiceNodeService, data_service::SendDataServiceRegisterPlayerZone};
    gRpcServiceRegistry[DataServiceGetPlayerHomeZoneMessageId] = RpcService{"DataService", "GetPlayerHomeZone", std::make_unique_for_overwrite<::data_service::GetPlayerHomeZoneRequest>(), std::make_unique_for_overwrite<::data_service::GetPlayerHomeZoneResponse>(), nullptr, 0, eNodeType::DataServiceNodeService, data_service::SendDataServiceGetPlayerHomeZone};
    gRpcServiceRegistry[DataServiceBatchGetPlayerHomeZoneMessageId] = RpcService{"DataService", "BatchGetPlayerHomeZone", std::make_unique_for_overwrite<::data_service::BatchGetPlayerHomeZoneRequest>(), std::make_unique_for_overwrite<::data_service::BatchGetPlayerHomeZoneResponse>(), nullptr, 0, eNodeType::DataServiceNodeService, data_service::SendDataServiceBatchGetPlayerHomeZone};
    gRpcServiceRegistry[DataServiceDeletePlayerDataMessageId] = RpcService{"DataService", "DeletePlayerData", std::make_unique_for_overwrite<::data_service::DeletePlayerDataRequest>(), std::make_unique_for_overwrite<::data_service::DeletePlayerDataResponse>(), nullptr, 0, eNodeType::DataServiceNodeService, data_service::SendDataServiceDeletePlayerData};
    gRpcServiceRegistry[KVRangeMessageId] = RpcService{"KV", "Range", std::make_unique_for_overwrite<::etcdserverpb::RangeRequest>(), std::make_unique_for_overwrite<::etcdserverpb::RangeResponse>(), nullptr, 0, eNodeType::EtcdNodeService, etcdserverpb::SendKVRange};
    gRpcServiceRegistry[KVPutMessageId] = RpcService{"KV", "Put", std::make_unique_for_overwrite<::etcdserverpb::PutRequest>(), std::make_unique_for_overwrite<::etcdserverpb::PutResponse>(), nullptr, 0, eNodeType::EtcdNodeService, etcdserverpb::SendKVPut};
    gRpcServiceRegistry[KVDeleteRangeMessageId] = RpcService{"KV", "DeleteRange", std::make_unique_for_overwrite<::etcdserverpb::DeleteRangeRequest>(), std::make_unique_for_overwrite<::etcdserverpb::DeleteRangeResponse>(), nullptr, 0, eNodeType::EtcdNodeService, etcdserverpb::SendKVDeleteRange};
    gRpcServiceRegistry[KVTxnMessageId] = RpcService{"KV", "Txn", std::make_unique_for_overwrite<::etcdserverpb::TxnRequest>(), std::make_unique_for_overwrite<::etcdserverpb::TxnResponse>(), nullptr, 0, eNodeType::EtcdNodeService, etcdserverpb::SendKVTxn};
    gRpcServiceRegistry[KVCompactMessageId] = RpcService{"KV", "Compact", std::make_unique_for_overwrite<::etcdserverpb::CompactionRequest>(), std::make_unique_for_overwrite<::etcdserverpb::CompactionResponse>(), nullptr, 0, eNodeType::EtcdNodeService, etcdserverpb::SendKVCompact};
    gRpcServiceRegistry[WatchWatchMessageId] = RpcService{"Watch", "Watch", std::make_unique_for_overwrite<::etcdserverpb::WatchRequest>(), std::make_unique_for_overwrite<::etcdserverpb::WatchResponse>(), nullptr, 0, eNodeType::EtcdNodeService, etcdserverpb::SendWatchWatch};
    gRpcServiceRegistry[LeaseLeaseGrantMessageId] = RpcService{"Lease", "LeaseGrant", std::make_unique_for_overwrite<::etcdserverpb::LeaseGrantRequest>(), std::make_unique_for_overwrite<::etcdserverpb::LeaseGrantResponse>(), nullptr, 0, eNodeType::EtcdNodeService, etcdserverpb::SendLeaseLeaseGrant};
    gRpcServiceRegistry[LeaseLeaseRevokeMessageId] = RpcService{"Lease", "LeaseRevoke", std::make_unique_for_overwrite<::etcdserverpb::LeaseRevokeRequest>(), std::make_unique_for_overwrite<::etcdserverpb::LeaseRevokeResponse>(), nullptr, 0, eNodeType::EtcdNodeService, etcdserverpb::SendLeaseLeaseRevoke};
    gRpcServiceRegistry[LeaseLeaseKeepAliveMessageId] = RpcService{"Lease", "LeaseKeepAlive", std::make_unique_for_overwrite<::etcdserverpb::LeaseKeepAliveRequest>(), std::make_unique_for_overwrite<::etcdserverpb::LeaseKeepAliveResponse>(), nullptr, 0, eNodeType::EtcdNodeService, etcdserverpb::SendLeaseLeaseKeepAlive};
    gRpcServiceRegistry[LeaseLeaseTimeToLiveMessageId] = RpcService{"Lease", "LeaseTimeToLive", std::make_unique_for_overwrite<::etcdserverpb::LeaseTimeToLiveRequest>(), std::make_unique_for_overwrite<::etcdserverpb::LeaseTimeToLiveResponse>(), nullptr, 0, eNodeType::EtcdNodeService, etcdserverpb::SendLeaseLeaseTimeToLive};
    gRpcServiceRegistry[LeaseLeaseLeasesMessageId] = RpcService{"Lease", "LeaseLeases", std::make_unique_for_overwrite<::etcdserverpb::LeaseLeasesRequest>(), std::make_unique_for_overwrite<::etcdserverpb::LeaseLeasesResponse>(), nullptr, 0, eNodeType::EtcdNodeService, etcdserverpb::SendLeaseLeaseLeases};
    gRpcServiceRegistry[GatePlayerEnterGameNodeMessageId] = RpcService{"Gate", "PlayerEnterGameNode", std::make_unique_for_overwrite<::RegisterGameNodeSessionRequest>(), std::make_unique_for_overwrite<::RegisterGameNodeSessionResponse>(), std::make_unique_for_overwrite<GateImpl>(), 0, eNodeType::GateNodeService};
    gRpcServiceRegistry[GateSendMessageToPlayerMessageId] = RpcService{"Gate", "SendMessageToPlayer", std::make_unique_for_overwrite<::NodeRouteMessageRequest>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<GateImpl>(), 0, eNodeType::GateNodeService};
    gRpcServiceRegistry[GateKickSessionByCentreMessageId] = RpcService{"Gate", "KickSessionByCentre", std::make_unique_for_overwrite<::KickSessionRequest>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<GateImpl>(), 0, eNodeType::GateNodeService};
    gRpcServiceRegistry[GateRouteNodeMessageMessageId] = RpcService{"Gate", "RouteNodeMessage", std::make_unique_for_overwrite<::RouteMessageRequest>(), std::make_unique_for_overwrite<::RouteMessageResponse>(), std::make_unique_for_overwrite<GateImpl>(), 0, eNodeType::GateNodeService};
    gRpcServiceRegistry[GateRoutePlayerMessageMessageId] = RpcService{"Gate", "RoutePlayerMessage", std::make_unique_for_overwrite<::RoutePlayerMessageRequest>(), std::make_unique_for_overwrite<::RoutePlayerMessageResponse>(), std::make_unique_for_overwrite<GateImpl>(), 0, eNodeType::GateNodeService};
    gRpcServiceRegistry[GateBroadcastToPlayersMessageId] = RpcService{"Gate", "BroadcastToPlayers", std::make_unique_for_overwrite<::BroadcastToPlayersRequest>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<GateImpl>(), 0, eNodeType::GateNodeService};
    gRpcServiceRegistry[GateNodeHandshakeMessageId] = RpcService{"Gate", "NodeHandshake", std::make_unique_for_overwrite<::NodeHandshakeRequest>(), std::make_unique_for_overwrite<::NodeHandshakeResponse>(), std::make_unique_for_overwrite<GateImpl>(), 0, eNodeType::GateNodeService};
    gRpcServiceRegistry[GateBindSessionToGateMessageId] = RpcService{"Gate", "BindSessionToGate", std::make_unique_for_overwrite<::BindSessionToGateRequest>(), std::make_unique_for_overwrite<::BindSessionToGateResponse>(), std::make_unique_for_overwrite<GateImpl>(), 0, eNodeType::GateNodeService};
    gRpcServiceRegistry[ClientPlayerLoginLoginMessageId] = RpcService{"ClientPlayerLogin", "Login", std::make_unique_for_overwrite<::loginpb::LoginRequest>(), std::make_unique_for_overwrite<::loginpb::LoginResponse>(), nullptr, 0, eNodeType::LoginNodeService, loginpb::SendClientPlayerLoginLogin};
    gRpcServiceRegistry[ClientPlayerLoginCreatePlayerMessageId] = RpcService{"ClientPlayerLogin", "CreatePlayer", std::make_unique_for_overwrite<::loginpb::CreatePlayerRequest>(), std::make_unique_for_overwrite<::loginpb::CreatePlayerResponse>(), nullptr, 0, eNodeType::LoginNodeService, loginpb::SendClientPlayerLoginCreatePlayer};
    gRpcServiceRegistry[ClientPlayerLoginEnterGameMessageId] = RpcService{"ClientPlayerLogin", "EnterGame", std::make_unique_for_overwrite<::loginpb::EnterGameRequest>(), std::make_unique_for_overwrite<::loginpb::EnterGameResponse>(), nullptr, 0, eNodeType::LoginNodeService, loginpb::SendClientPlayerLoginEnterGame};
    gRpcServiceRegistry[ClientPlayerLoginLeaveGameMessageId] = RpcService{"ClientPlayerLogin", "LeaveGame", std::make_unique_for_overwrite<::loginpb::LeaveGameRequest>(), std::make_unique_for_overwrite<::loginpb::LoginEmptyResponse>(), nullptr, 0, eNodeType::LoginNodeService, loginpb::SendClientPlayerLoginLeaveGame};
    gRpcServiceRegistry[ClientPlayerLoginDisconnectMessageId] = RpcService{"ClientPlayerLogin", "Disconnect", std::make_unique_for_overwrite<::loginpb::LoginNodeDisconnectRequest>(), std::make_unique_for_overwrite<::loginpb::LoginEmptyResponse>(), nullptr, 0, eNodeType::LoginNodeService, loginpb::SendClientPlayerLoginDisconnect};
    gRpcServiceRegistry[SceneClientPlayerCommonSendTipToClientMessageId] = RpcService{"SceneClientPlayerCommon", "SendTipToClient", std::make_unique_for_overwrite<::TipInfoMessage>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<SceneClientPlayerCommonImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[SceneClientPlayerCommonKickPlayerMessageId] = RpcService{"SceneClientPlayerCommon", "KickPlayer", std::make_unique_for_overwrite<::GameKickPlayerRequest>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<SceneClientPlayerCommonImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[ScenePlayerCentre2GsLoginMessageId] = RpcService{"ScenePlayer", "Centre2GsLogin", std::make_unique_for_overwrite<::Centre2GsLoginRequest>(), std::make_unique_for_overwrite<::google::protobuf::Empty>(), std::make_unique_for_overwrite<ScenePlayerImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[ScenePlayerExitGameMessageId] = RpcService{"ScenePlayer", "ExitGame", std::make_unique_for_overwrite<::GameNodeExitGameRequest>(), std::make_unique_for_overwrite<::google::protobuf::Empty>(), std::make_unique_for_overwrite<ScenePlayerImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[SceneScenePlayerEnterSceneMessageId] = RpcService{"SceneScenePlayer", "EnterScene", std::make_unique_for_overwrite<::GsEnterSceneRequest>(), std::make_unique_for_overwrite<::google::protobuf::Empty>(), std::make_unique_for_overwrite<SceneScenePlayerImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[SceneScenePlayerLeaveSceneMessageId] = RpcService{"SceneScenePlayer", "LeaveScene", std::make_unique_for_overwrite<::GsLeaveSceneRequest>(), std::make_unique_for_overwrite<::google::protobuf::Empty>(), std::make_unique_for_overwrite<SceneScenePlayerImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[SceneScenePlayerEnterSceneS2CMessageId] = RpcService{"SceneScenePlayer", "EnterSceneS2C", std::make_unique_for_overwrite<::EnterSceneS2CRequest>(), std::make_unique_for_overwrite<::EnterScenerS2CResponse>(), std::make_unique_for_overwrite<SceneScenePlayerImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[SceneSceneTestMessageId] = RpcService{"SceneScene", "Test", std::make_unique_for_overwrite<::GameSceneTest>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<SceneSceneImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[SceneSceneClientPlayerEnterSceneMessageId] = RpcService{"SceneSceneClientPlayer", "EnterScene", std::make_unique_for_overwrite<::EnterSceneC2SRequest>(), std::make_unique_for_overwrite<::EnterSceneC2SResponse>(), std::make_unique_for_overwrite<SceneSceneClientPlayerImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[SceneSceneClientPlayerNotifyEnterSceneMessageId] = RpcService{"SceneSceneClientPlayer", "NotifyEnterScene", std::make_unique_for_overwrite<::EnterSceneS2C>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<SceneSceneClientPlayerImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[SceneSceneClientPlayerSceneInfoC2SMessageId] = RpcService{"SceneSceneClientPlayer", "SceneInfoC2S", std::make_unique_for_overwrite<::SceneInfoRequest>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<SceneSceneClientPlayerImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[SceneSceneClientPlayerNotifySceneInfoMessageId] = RpcService{"SceneSceneClientPlayer", "NotifySceneInfo", std::make_unique_for_overwrite<::SceneInfoS2C>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<SceneSceneClientPlayerImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[SceneSceneClientPlayerNotifyActorCreateMessageId] = RpcService{"SceneSceneClientPlayer", "NotifyActorCreate", std::make_unique_for_overwrite<::ActorCreateS2C>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<SceneSceneClientPlayerImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[SceneSceneClientPlayerNotifyActorDestroyMessageId] = RpcService{"SceneSceneClientPlayer", "NotifyActorDestroy", std::make_unique_for_overwrite<::ActorDestroyS2C>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<SceneSceneClientPlayerImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[SceneSceneClientPlayerNotifyActorListCreateMessageId] = RpcService{"SceneSceneClientPlayer", "NotifyActorListCreate", std::make_unique_for_overwrite<::ActorListCreateS2C>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<SceneSceneClientPlayerImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[SceneSceneClientPlayerNotifyActorListDestroyMessageId] = RpcService{"SceneSceneClientPlayer", "NotifyActorListDestroy", std::make_unique_for_overwrite<::ActorListDestroyS2C>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<SceneSceneClientPlayerImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[SceneSkillClientPlayerReleaseSkillMessageId] = RpcService{"SceneSkillClientPlayer", "ReleaseSkill", std::make_unique_for_overwrite<::ReleaseSkillSkillRequest>(), std::make_unique_for_overwrite<::ReleaseSkillSkillResponse>(), std::make_unique_for_overwrite<SceneSkillClientPlayerImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[SceneSkillClientPlayerNotifySkillUsedMessageId] = RpcService{"SceneSkillClientPlayer", "NotifySkillUsed", std::make_unique_for_overwrite<::SkillUsedS2C>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<SceneSkillClientPlayerImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[SceneSkillClientPlayerNotifySkillInterruptedMessageId] = RpcService{"SceneSkillClientPlayer", "NotifySkillInterrupted", std::make_unique_for_overwrite<::SkillInterruptedS2C>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<SceneSkillClientPlayerImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[SceneSkillClientPlayerGetSkillListMessageId] = RpcService{"SceneSkillClientPlayer", "GetSkillList", std::make_unique_for_overwrite<::GetSkillListRequest>(), std::make_unique_for_overwrite<::GetSkillListResponse>(), std::make_unique_for_overwrite<SceneSkillClientPlayerImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[ScenePlayerSyncSyncBaseAttributeMessageId] = RpcService{"ScenePlayerSync", "SyncBaseAttribute", std::make_unique_for_overwrite<::ActorBaseAttributesS2C>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<ScenePlayerSyncImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[ScenePlayerSyncSyncAttribute2FramesMessageId] = RpcService{"ScenePlayerSync", "SyncAttribute2Frames", std::make_unique_for_overwrite<::AttributeDelta2FramesS2C>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<ScenePlayerSyncImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[ScenePlayerSyncSyncAttribute5FramesMessageId] = RpcService{"ScenePlayerSync", "SyncAttribute5Frames", std::make_unique_for_overwrite<::AttributeDelta5FramesS2C>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<ScenePlayerSyncImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[ScenePlayerSyncSyncAttribute10FramesMessageId] = RpcService{"ScenePlayerSync", "SyncAttribute10Frames", std::make_unique_for_overwrite<::AttributeDelta10FramesS2C>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<ScenePlayerSyncImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[ScenePlayerSyncSyncAttribute30FramesMessageId] = RpcService{"ScenePlayerSync", "SyncAttribute30Frames", std::make_unique_for_overwrite<::AttributeDelta30FramesS2C>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<ScenePlayerSyncImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[ScenePlayerSyncSyncAttribute60FramesMessageId] = RpcService{"ScenePlayerSync", "SyncAttribute60Frames", std::make_unique_for_overwrite<::AttributeDelta60FramesS2C>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<ScenePlayerSyncImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[ScenePlayerEnterGameNodeMessageId] = RpcService{"Scene", "PlayerEnterGameNode", std::make_unique_for_overwrite<::PlayerEnterGameNodeRequest>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<SceneImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[SceneSendMessageToPlayerMessageId] = RpcService{"Scene", "SendMessageToPlayer", std::make_unique_for_overwrite<::NodeRouteMessageRequest>(), std::make_unique_for_overwrite<::NodeRouteMessageResponse>(), std::make_unique_for_overwrite<SceneImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[SceneProcessClientPlayerMessageMessageId] = RpcService{"Scene", "ProcessClientPlayerMessage", std::make_unique_for_overwrite<::ProcessClientPlayerMessageRequest>(), std::make_unique_for_overwrite<::ProcessClientPlayerMessageResponse>(), std::make_unique_for_overwrite<SceneImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[SceneCentreSendToPlayerViaGameNodeMessageId] = RpcService{"Scene", "CentreSendToPlayerViaGameNode", std::make_unique_for_overwrite<::NodeRouteMessageRequest>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<SceneImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[SceneInvokePlayerServiceMessageId] = RpcService{"Scene", "InvokePlayerService", std::make_unique_for_overwrite<::NodeRouteMessageRequest>(), std::make_unique_for_overwrite<::NodeRouteMessageResponse>(), std::make_unique_for_overwrite<SceneImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[SceneRouteNodeStringMsgMessageId] = RpcService{"Scene", "RouteNodeStringMsg", std::make_unique_for_overwrite<::RouteMessageRequest>(), std::make_unique_for_overwrite<::RouteMessageResponse>(), std::make_unique_for_overwrite<SceneImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[SceneRoutePlayerStringMsgMessageId] = RpcService{"Scene", "RoutePlayerStringMsg", std::make_unique_for_overwrite<::RoutePlayerMessageRequest>(), std::make_unique_for_overwrite<::RoutePlayerMessageResponse>(), std::make_unique_for_overwrite<SceneImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[SceneUpdateSessionDetailMessageId] = RpcService{"Scene", "UpdateSessionDetail", std::make_unique_for_overwrite<::RegisterPlayerSessionRequest>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<SceneImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[SceneEnterSceneMessageId] = RpcService{"Scene", "EnterScene", std::make_unique_for_overwrite<::Centre2GsEnterSceneRequest>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<SceneImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[SceneCreateSceneMessageId] = RpcService{"Scene", "CreateScene", std::make_unique_for_overwrite<::CreateSceneRequest>(), std::make_unique_for_overwrite<::CreateSceneResponse>(), std::make_unique_for_overwrite<SceneImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[SceneNodeHandshakeMessageId] = RpcService{"Scene", "NodeHandshake", std::make_unique_for_overwrite<::NodeHandshakeRequest>(), std::make_unique_for_overwrite<::NodeHandshakeResponse>(), std::make_unique_for_overwrite<SceneImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[SceneManagerCreateSceneMessageId] = RpcService{"SceneManager", "CreateScene", std::make_unique_for_overwrite<::scene_manager::CreateSceneRequest>(), std::make_unique_for_overwrite<::scene_manager::CreateSceneResponse>(), nullptr, 0, eNodeType::SceneManagerNodeService, scene_manager::SendSceneManagerCreateScene};
    gRpcServiceRegistry[SceneManagerDestroySceneMessageId] = RpcService{"SceneManager", "DestroyScene", std::make_unique_for_overwrite<::scene_manager::DestroySceneRequest>(), std::make_unique_for_overwrite<::Empty>(), nullptr, 0, eNodeType::SceneManagerNodeService, scene_manager::SendSceneManagerDestroyScene};
    gRpcServiceRegistry[SceneManagerEnterSceneByCentreMessageId] = RpcService{"SceneManager", "EnterSceneByCentre", std::make_unique_for_overwrite<::scene_manager::EnterSceneByCentreRequest>(), std::make_unique_for_overwrite<::scene_manager::EnterSceneByCentreResponse>(), nullptr, 0, eNodeType::SceneManagerNodeService, scene_manager::SendSceneManagerEnterSceneByCentre};
    gRpcServiceRegistry[SceneManagerLeaveSceneByCentreMessageId] = RpcService{"SceneManager", "LeaveSceneByCentre", std::make_unique_for_overwrite<::scene_manager::LeaveSceneByCentreRequest>(), std::make_unique_for_overwrite<::Empty>(), nullptr, 0, eNodeType::SceneManagerNodeService, scene_manager::SendSceneManagerLeaveSceneByCentre};

    gClientMessageIdWhitelist.emplace(ClientPlayerLoginLoginMessageId);
    gClientMessageIdWhitelist.emplace(ClientPlayerLoginCreatePlayerMessageId);
    gClientMessageIdWhitelist.emplace(ClientPlayerLoginEnterGameMessageId);
    gClientMessageIdWhitelist.emplace(ClientPlayerLoginLeaveGameMessageId);
    gClientMessageIdWhitelist.emplace(ClientPlayerLoginDisconnectMessageId);
    gClientMessageIdWhitelist.emplace(SceneClientPlayerCommonSendTipToClientMessageId);
    gClientMessageIdWhitelist.emplace(SceneClientPlayerCommonKickPlayerMessageId);
    gClientMessageIdWhitelist.emplace(SceneSceneClientPlayerEnterSceneMessageId);
    gClientMessageIdWhitelist.emplace(SceneSceneClientPlayerNotifyEnterSceneMessageId);
    gClientMessageIdWhitelist.emplace(SceneSceneClientPlayerSceneInfoC2SMessageId);
    gClientMessageIdWhitelist.emplace(SceneSceneClientPlayerNotifySceneInfoMessageId);
    gClientMessageIdWhitelist.emplace(SceneSceneClientPlayerNotifyActorCreateMessageId);
    gClientMessageIdWhitelist.emplace(SceneSceneClientPlayerNotifyActorDestroyMessageId);
    gClientMessageIdWhitelist.emplace(SceneSceneClientPlayerNotifyActorListCreateMessageId);
    gClientMessageIdWhitelist.emplace(SceneSceneClientPlayerNotifyActorListDestroyMessageId);
    gClientMessageIdWhitelist.emplace(SceneSkillClientPlayerReleaseSkillMessageId);
    gClientMessageIdWhitelist.emplace(SceneSkillClientPlayerNotifySkillUsedMessageId);
    gClientMessageIdWhitelist.emplace(SceneSkillClientPlayerNotifySkillInterruptedMessageId);
    gClientMessageIdWhitelist.emplace(SceneSkillClientPlayerGetSkillListMessageId);
}

void InitEventInfo()
{
	gProtoEventRegistry[AcceptMissionEventEventId] = ProtoEvent{"AcceptMissionEvent", std::make_unique_for_overwrite<AcceptMissionEvent>(), &DispatchAcceptMissionEvent};
	gProtoEventRegistry[AfterEnterSceneEventId] = ProtoEvent{"AfterEnterScene", std::make_unique_for_overwrite<AfterEnterScene>(), &DispatchAfterEnterScene};
	gProtoEventRegistry[AfterLeaveSceneEventId] = ProtoEvent{"AfterLeaveScene", std::make_unique_for_overwrite<AfterLeaveScene>(), &DispatchAfterLeaveScene};
	gProtoEventRegistry[BeKillEventEventId] = ProtoEvent{"BeKillEvent", std::make_unique_for_overwrite<BeKillEvent>(), &DispatchBeKillEvent};
	gProtoEventRegistry[BeforeEnterSceneEventId] = ProtoEvent{"BeforeEnterScene", std::make_unique_for_overwrite<BeforeEnterScene>(), &DispatchBeforeEnterScene};
	gProtoEventRegistry[BeforeLeaveSceneEventId] = ProtoEvent{"BeforeLeaveScene", std::make_unique_for_overwrite<BeforeLeaveScene>(), &DispatchBeforeLeaveScene};
	gProtoEventRegistry[BuffTestEvetEventId] = ProtoEvent{"BuffTestEvet", std::make_unique_for_overwrite<BuffTestEvet>(), &DispatchBuffTestEvet};
	gProtoEventRegistry[CombatStateAddedPbEventEventId] = ProtoEvent{"CombatStateAddedPbEvent", std::make_unique_for_overwrite<CombatStateAddedPbEvent>(), &DispatchCombatStateAddedPbEvent};
	gProtoEventRegistry[CombatStateRemovedPbEventEventId] = ProtoEvent{"CombatStateRemovedPbEvent", std::make_unique_for_overwrite<CombatStateRemovedPbEvent>(), &DispatchCombatStateRemovedPbEvent};
	gProtoEventRegistry[ConnectToNodePbEventEventId] = ProtoEvent{"ConnectToNodePbEvent", std::make_unique_for_overwrite<ConnectToNodePbEvent>(), &DispatchConnectToNodePbEvent};
	gProtoEventRegistry[ContractsKafkaKickPlayerEventEventId] = ProtoEvent{"contracts::kafka::KickPlayerEvent", std::make_unique_for_overwrite<contracts::kafka::KickPlayerEvent>(), &DispatchContractsKafkaKickPlayerEvent};
	gProtoEventRegistry[ContractsKafkaRoutePlayerEventEventId] = ProtoEvent{"contracts::kafka::RoutePlayerEvent", std::make_unique_for_overwrite<contracts::kafka::RoutePlayerEvent>(), &DispatchContractsKafkaRoutePlayerEvent};
	gProtoEventRegistry[InitializeActorComponentsEventEventId] = ProtoEvent{"InitializeActorComponentsEvent", std::make_unique_for_overwrite<InitializeActorComponentsEvent>(), &DispatchInitializeActorComponentsEvent};
	gProtoEventRegistry[InitializeNpcComponentsEventEventId] = ProtoEvent{"InitializeNpcComponentsEvent", std::make_unique_for_overwrite<InitializeNpcComponentsEvent>(), &DispatchInitializeNpcComponentsEvent};
	gProtoEventRegistry[InitializePlayerComponentsEventEventId] = ProtoEvent{"InitializePlayerComponentsEvent", std::make_unique_for_overwrite<InitializePlayerComponentsEvent>(), &DispatchInitializePlayerComponentsEvent};
	gProtoEventRegistry[InterruptCurrentStatePbEventEventId] = ProtoEvent{"InterruptCurrentStatePbEvent", std::make_unique_for_overwrite<InterruptCurrentStatePbEvent>(), &DispatchInterruptCurrentStatePbEvent};
	gProtoEventRegistry[MissionConditionEventEventId] = ProtoEvent{"MissionConditionEvent", std::make_unique_for_overwrite<MissionConditionEvent>(), &DispatchMissionConditionEvent};
	gProtoEventRegistry[OnAcceptedMissionEventEventId] = ProtoEvent{"OnAcceptedMissionEvent", std::make_unique_for_overwrite<OnAcceptedMissionEvent>(), &DispatchOnAcceptedMissionEvent};
	gProtoEventRegistry[OnConnect2CentrePbEventEventId] = ProtoEvent{"OnConnect2CentrePbEvent", std::make_unique_for_overwrite<OnConnect2CentrePbEvent>(), &DispatchOnConnect2CentrePbEvent};
	gProtoEventRegistry[OnConnect2LoginEventId] = ProtoEvent{"OnConnect2Login", std::make_unique_for_overwrite<OnConnect2Login>(), &DispatchOnConnect2Login};
	gProtoEventRegistry[OnMissionAwardEventEventId] = ProtoEvent{"OnMissionAwardEvent", std::make_unique_for_overwrite<OnMissionAwardEvent>(), &DispatchOnMissionAwardEvent};
	gProtoEventRegistry[OnNodeAddPbEventEventId] = ProtoEvent{"OnNodeAddPbEvent", std::make_unique_for_overwrite<OnNodeAddPbEvent>(), &DispatchOnNodeAddPbEvent};
	gProtoEventRegistry[OnNodeConnectedPbEventEventId] = ProtoEvent{"OnNodeConnectedPbEvent", std::make_unique_for_overwrite<OnNodeConnectedPbEvent>(), &DispatchOnNodeConnectedPbEvent};
	gProtoEventRegistry[OnNodeRemovePbEventEventId] = ProtoEvent{"OnNodeRemovePbEvent", std::make_unique_for_overwrite<OnNodeRemovePbEvent>(), &DispatchOnNodeRemovePbEvent};
	gProtoEventRegistry[OnSceneCreatedEventId] = ProtoEvent{"OnSceneCreated", std::make_unique_for_overwrite<OnSceneCreated>(), &DispatchOnSceneCreated};
	gProtoEventRegistry[OnSceneDestroyedEventId] = ProtoEvent{"OnSceneDestroyed", std::make_unique_for_overwrite<OnSceneDestroyed>(), &DispatchOnSceneDestroyed};
	gProtoEventRegistry[OnServerStartEventId] = ProtoEvent{"OnServerStart", std::make_unique_for_overwrite<OnServerStart>(), &DispatchOnServerStart};
	gProtoEventRegistry[PlayerMigrationPbEventEventId] = ProtoEvent{"PlayerMigrationPbEvent", std::make_unique_for_overwrite<PlayerMigrationPbEvent>(), &DispatchPlayerMigrationPbEvent};
	gProtoEventRegistry[PlayerUpgradeEventEventId] = ProtoEvent{"PlayerUpgradeEvent", std::make_unique_for_overwrite<PlayerUpgradeEvent>(), &DispatchPlayerUpgradeEvent};
	gProtoEventRegistry[RegisterPlayerEventEventId] = ProtoEvent{"RegisterPlayerEvent", std::make_unique_for_overwrite<RegisterPlayerEvent>(), &DispatchRegisterPlayerEvent};
	gProtoEventRegistry[S2CEnterSceneEventId] = ProtoEvent{"S2CEnterScene", std::make_unique_for_overwrite<S2CEnterScene>(), &DispatchS2CEnterScene};
	gProtoEventRegistry[SkillExecutedEventEventId] = ProtoEvent{"SkillExecutedEvent", std::make_unique_for_overwrite<SkillExecutedEvent>(), &DispatchSkillExecutedEvent};
}

bool IsValidEventId(uint32_t eventId)
{
	if (eventId >= gProtoEventRegistry.size()) {
		return false;
	}
	return gProtoEventRegistry[eventId].prototype != nullptr;
}

MessageUniquePtr NewEventMessage(uint32_t eventId)
{
	if (!IsValidEventId(eventId)) {
		return nullptr;
	}

	return MessageUniquePtr(gProtoEventRegistry[eventId].prototype->New());
}

MessageUniquePtr ParseEventMessage(uint32_t eventId, const std::string& payload)
{
	auto message = NewEventMessage(eventId);
	if (!message) {
		return nullptr;
	}

	if (!message->ParseFromString(payload)) {
		return nullptr;
	}

	return message;
}

void DispatchProtoEvent(uint32_t eventId, const google::protobuf::Message& message)
{
	if (!IsValidEventId(eventId)) {
		return;
	}

	auto dispatchFn = gProtoEventRegistry[eventId].dispatcher;
	if (dispatchFn == nullptr) {
		return;
	}

	dispatchFn(message);
}
