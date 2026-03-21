#include <array>
#include "service_metadata.h"
#include "proto/common/base/node.pb.h"
#include "thread_context/dispatcher_manager.h"

#include "proto/data_service/data_service.pb.h"
#include "proto/etcd/etcd.pb.h"
#include "proto/gate/gate_service.pb.h"
#include "proto/login/login.pb.h"
#include "proto/scene/game_client_player.pb.h"
#include "proto/scene/game_player.pb.h"
#include "proto/scene/game_player_scene.pb.h"
#include "proto/scene/game_scene.pb.h"
#include "proto/scene/player_scene.pb.h"
#include "proto/scene/player_skill.pb.h"
#include "proto/scene/player_state_attribute_sync.pb.h"
#include "proto/scene/scene.pb.h"
#include "proto/scene_manager/scene_manager_service.pb.h"

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
#include "proto/contracts/kafka/player_event.pb.h"
#include "proto/common/event/actor_event.pb.h"
#include "proto/common/event/npc_event.pb.h"
#include "proto/common/event/player_event.pb.h"
#include "proto/common/event/server_event.pb.h"
#include "proto/common/event/player_migration_event.pb.h"
#include "proto/common/event/skill_event.pb.h"
#include "common_event_mission_event_event_id.h"
#include "common_event_scene_event_event_id.h"
#include "common_event_combat_event_event_id.h"
#include "common_event_buff_event_event_id.h"
#include "common_event_actor_combat_state_event_event_id.h"
#include "common_event_node_event_event_id.h"
#include "contracts_kafka_gate_event_event_id.h"
#include "contracts_kafka_player_event_event_id.h"
#include "common_event_actor_event_event_id.h"
#include "common_event_npc_event_event_id.h"
#include "common_event_player_event_event_id.h"
#include "common_event_server_event_event_id.h"
#include "common_event_player_migration_event_event_id.h"
#include "common_event_skill_event_event_id.h"

class GateImpl final : public Gate {};
class SceneClientPlayerCommonImpl final : public SceneClientPlayerCommon {};
class ScenePlayerImpl final : public ScenePlayer {};
class SceneScenePlayerImpl final : public SceneScenePlayer {};
class SceneSceneImpl final : public SceneScene {};
class SceneSceneClientPlayerImpl final : public SceneSceneClientPlayer {};
class SceneSkillClientPlayerImpl final : public SceneSkillClientPlayer {};
class ScenePlayerSyncImpl final : public ScenePlayerSync {};
class SceneImpl final : public Scene {};

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
namespace scene_manager{void SendSceneManagerEnterScene(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace scene_manager{void SendSceneManagerLeaveScene(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}

std::unordered_set<uint32_t> gClientMessageIdWhitelist;
std::array<RpcService, 94> gRpcServiceRegistry;
std::array<std::unique_ptr<google::protobuf::Message>, 36> gEventPrototypes;

void InitMessageInfo()
{
    gRpcServiceRegistry[DataServiceLoadPlayerDataMessageId] = RpcService{"DataService", "LoadPlayerData", std::make_unique<::data_service::LoadPlayerDataRequest>(), std::make_unique<::data_service::LoadPlayerDataResponse>(), nullptr, 0, eNodeType::DataServiceNodeService, data_service::SendDataServiceLoadPlayerData};
    gRpcServiceRegistry[DataServiceSavePlayerDataMessageId] = RpcService{"DataService", "SavePlayerData", std::make_unique<::data_service::SavePlayerDataRequest>(), std::make_unique<::data_service::SavePlayerDataResponse>(), nullptr, 0, eNodeType::DataServiceNodeService, data_service::SendDataServiceSavePlayerData};
    gRpcServiceRegistry[DataServiceGetPlayerFieldMessageId] = RpcService{"DataService", "GetPlayerField", std::make_unique<::data_service::GetPlayerFieldRequest>(), std::make_unique<::data_service::GetPlayerFieldResponse>(), nullptr, 0, eNodeType::DataServiceNodeService, data_service::SendDataServiceGetPlayerField};
    gRpcServiceRegistry[DataServiceSetPlayerFieldMessageId] = RpcService{"DataService", "SetPlayerField", std::make_unique<::data_service::SetPlayerFieldRequest>(), std::make_unique<::data_service::SetPlayerFieldResponse>(), nullptr, 0, eNodeType::DataServiceNodeService, data_service::SendDataServiceSetPlayerField};
    gRpcServiceRegistry[DataServiceRegisterPlayerZoneMessageId] = RpcService{"DataService", "RegisterPlayerZone", std::make_unique<::data_service::RegisterPlayerZoneRequest>(), std::make_unique<::google::protobuf::Empty>(), nullptr, 0, eNodeType::DataServiceNodeService, data_service::SendDataServiceRegisterPlayerZone};
    gRpcServiceRegistry[DataServiceGetPlayerHomeZoneMessageId] = RpcService{"DataService", "GetPlayerHomeZone", std::make_unique<::data_service::GetPlayerHomeZoneRequest>(), std::make_unique<::data_service::GetPlayerHomeZoneResponse>(), nullptr, 0, eNodeType::DataServiceNodeService, data_service::SendDataServiceGetPlayerHomeZone};
    gRpcServiceRegistry[DataServiceBatchGetPlayerHomeZoneMessageId] = RpcService{"DataService", "BatchGetPlayerHomeZone", std::make_unique<::data_service::BatchGetPlayerHomeZoneRequest>(), std::make_unique<::data_service::BatchGetPlayerHomeZoneResponse>(), nullptr, 0, eNodeType::DataServiceNodeService, data_service::SendDataServiceBatchGetPlayerHomeZone};
    gRpcServiceRegistry[DataServiceDeletePlayerDataMessageId] = RpcService{"DataService", "DeletePlayerData", std::make_unique<::data_service::DeletePlayerDataRequest>(), std::make_unique<::data_service::DeletePlayerDataResponse>(), nullptr, 0, eNodeType::DataServiceNodeService, data_service::SendDataServiceDeletePlayerData};
    gRpcServiceRegistry[KVRangeMessageId] = RpcService{"KV", "Range", std::make_unique<::etcdserverpb::RangeRequest>(), std::make_unique<::etcdserverpb::RangeResponse>(), nullptr, 0, eNodeType::EtcdNodeService, etcdserverpb::SendKVRange};
    gRpcServiceRegistry[KVPutMessageId] = RpcService{"KV", "Put", std::make_unique<::etcdserverpb::PutRequest>(), std::make_unique<::etcdserverpb::PutResponse>(), nullptr, 0, eNodeType::EtcdNodeService, etcdserverpb::SendKVPut};
    gRpcServiceRegistry[KVDeleteRangeMessageId] = RpcService{"KV", "DeleteRange", std::make_unique<::etcdserverpb::DeleteRangeRequest>(), std::make_unique<::etcdserverpb::DeleteRangeResponse>(), nullptr, 0, eNodeType::EtcdNodeService, etcdserverpb::SendKVDeleteRange};
    gRpcServiceRegistry[KVTxnMessageId] = RpcService{"KV", "Txn", std::make_unique<::etcdserverpb::TxnRequest>(), std::make_unique<::etcdserverpb::TxnResponse>(), nullptr, 0, eNodeType::EtcdNodeService, etcdserverpb::SendKVTxn};
    gRpcServiceRegistry[KVCompactMessageId] = RpcService{"KV", "Compact", std::make_unique<::etcdserverpb::CompactionRequest>(), std::make_unique<::etcdserverpb::CompactionResponse>(), nullptr, 0, eNodeType::EtcdNodeService, etcdserverpb::SendKVCompact};
    gRpcServiceRegistry[WatchWatchMessageId] = RpcService{"Watch", "Watch", std::make_unique<::etcdserverpb::WatchRequest>(), std::make_unique<::etcdserverpb::WatchResponse>(), nullptr, 0, eNodeType::EtcdNodeService, etcdserverpb::SendWatchWatch};
    gRpcServiceRegistry[LeaseLeaseGrantMessageId] = RpcService{"Lease", "LeaseGrant", std::make_unique<::etcdserverpb::LeaseGrantRequest>(), std::make_unique<::etcdserverpb::LeaseGrantResponse>(), nullptr, 0, eNodeType::EtcdNodeService, etcdserverpb::SendLeaseLeaseGrant};
    gRpcServiceRegistry[LeaseLeaseRevokeMessageId] = RpcService{"Lease", "LeaseRevoke", std::make_unique<::etcdserverpb::LeaseRevokeRequest>(), std::make_unique<::etcdserverpb::LeaseRevokeResponse>(), nullptr, 0, eNodeType::EtcdNodeService, etcdserverpb::SendLeaseLeaseRevoke};
    gRpcServiceRegistry[LeaseLeaseKeepAliveMessageId] = RpcService{"Lease", "LeaseKeepAlive", std::make_unique<::etcdserverpb::LeaseKeepAliveRequest>(), std::make_unique<::etcdserverpb::LeaseKeepAliveResponse>(), nullptr, 0, eNodeType::EtcdNodeService, etcdserverpb::SendLeaseLeaseKeepAlive};
    gRpcServiceRegistry[LeaseLeaseTimeToLiveMessageId] = RpcService{"Lease", "LeaseTimeToLive", std::make_unique<::etcdserverpb::LeaseTimeToLiveRequest>(), std::make_unique<::etcdserverpb::LeaseTimeToLiveResponse>(), nullptr, 0, eNodeType::EtcdNodeService, etcdserverpb::SendLeaseLeaseTimeToLive};
    gRpcServiceRegistry[LeaseLeaseLeasesMessageId] = RpcService{"Lease", "LeaseLeases", std::make_unique<::etcdserverpb::LeaseLeasesRequest>(), std::make_unique<::etcdserverpb::LeaseLeasesResponse>(), nullptr, 0, eNodeType::EtcdNodeService, etcdserverpb::SendLeaseLeaseLeases};
    gRpcServiceRegistry[GatePlayerEnterGameNodeMessageId] = RpcService{"Gate", "PlayerEnterGameNode", std::make_unique<::RegisterGameNodeSessionRequest>(), std::make_unique<::RegisterGameNodeSessionResponse>(), std::make_unique<GateImpl>(), 0, eNodeType::GateNodeService};
    gRpcServiceRegistry[GateSendMessageToPlayerMessageId] = RpcService{"Gate", "SendMessageToPlayer", std::make_unique<::NodeRouteMessageRequest>(), std::make_unique<::Empty>(), std::make_unique<GateImpl>(), 0, eNodeType::GateNodeService};
    gRpcServiceRegistry[GateRouteNodeMessageMessageId] = RpcService{"Gate", "RouteNodeMessage", std::make_unique<::RouteMessageRequest>(), std::make_unique<::RouteMessageResponse>(), std::make_unique<GateImpl>(), 0, eNodeType::GateNodeService};
    gRpcServiceRegistry[GateRoutePlayerMessageMessageId] = RpcService{"Gate", "RoutePlayerMessage", std::make_unique<::RoutePlayerMessageRequest>(), std::make_unique<::RoutePlayerMessageResponse>(), std::make_unique<GateImpl>(), 0, eNodeType::GateNodeService};
    gRpcServiceRegistry[GateBroadcastToPlayersMessageId] = RpcService{"Gate", "BroadcastToPlayers", std::make_unique<::BroadcastToPlayersRequest>(), std::make_unique<::Empty>(), std::make_unique<GateImpl>(), 0, eNodeType::GateNodeService};
    gRpcServiceRegistry[GateNodeHandshakeMessageId] = RpcService{"Gate", "NodeHandshake", std::make_unique<::NodeHandshakeRequest>(), std::make_unique<::NodeHandshakeResponse>(), std::make_unique<GateImpl>(), 0, eNodeType::GateNodeService};
    gRpcServiceRegistry[GateBindSessionToGateMessageId] = RpcService{"Gate", "BindSessionToGate", std::make_unique<::BindSessionToGateRequest>(), std::make_unique<::BindSessionToGateResponse>(), std::make_unique<GateImpl>(), 0, eNodeType::GateNodeService};
    gRpcServiceRegistry[ClientPlayerLoginLoginMessageId] = RpcService{"ClientPlayerLogin", "Login", std::make_unique<::loginpb::LoginRequest>(), std::make_unique<::loginpb::LoginResponse>(), nullptr, 0, eNodeType::LoginNodeService, loginpb::SendClientPlayerLoginLogin};
    gRpcServiceRegistry[ClientPlayerLoginCreatePlayerMessageId] = RpcService{"ClientPlayerLogin", "CreatePlayer", std::make_unique<::loginpb::CreatePlayerRequest>(), std::make_unique<::loginpb::CreatePlayerResponse>(), nullptr, 0, eNodeType::LoginNodeService, loginpb::SendClientPlayerLoginCreatePlayer};
    gRpcServiceRegistry[ClientPlayerLoginEnterGameMessageId] = RpcService{"ClientPlayerLogin", "EnterGame", std::make_unique<::loginpb::EnterGameRequest>(), std::make_unique<::loginpb::EnterGameResponse>(), nullptr, 0, eNodeType::LoginNodeService, loginpb::SendClientPlayerLoginEnterGame};
    gRpcServiceRegistry[ClientPlayerLoginLeaveGameMessageId] = RpcService{"ClientPlayerLogin", "LeaveGame", std::make_unique<::loginpb::LeaveGameRequest>(), std::make_unique<::loginpb::LoginEmptyResponse>(), nullptr, 0, eNodeType::LoginNodeService, loginpb::SendClientPlayerLoginLeaveGame};
    gRpcServiceRegistry[ClientPlayerLoginDisconnectMessageId] = RpcService{"ClientPlayerLogin", "Disconnect", std::make_unique<::loginpb::LoginNodeDisconnectRequest>(), std::make_unique<::loginpb::LoginEmptyResponse>(), nullptr, 0, eNodeType::LoginNodeService, loginpb::SendClientPlayerLoginDisconnect};
    gRpcServiceRegistry[SceneClientPlayerCommonSendTipToClientMessageId] = RpcService{"SceneClientPlayerCommon", "SendTipToClient", std::make_unique<::TipInfoMessage>(), std::make_unique<::Empty>(), std::make_unique<SceneClientPlayerCommonImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[SceneClientPlayerCommonKickPlayerMessageId] = RpcService{"SceneClientPlayerCommon", "KickPlayer", std::make_unique<::GameKickPlayerRequest>(), std::make_unique<::Empty>(), std::make_unique<SceneClientPlayerCommonImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[ScenePlayerCentre2GsLoginMessageId] = RpcService{"ScenePlayer", "Centre2GsLogin", std::make_unique<::Centre2GsLoginRequest>(), std::make_unique<::google::protobuf::Empty>(), std::make_unique<ScenePlayerImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[ScenePlayerExitGameMessageId] = RpcService{"ScenePlayer", "ExitGame", std::make_unique<::GameNodeExitGameRequest>(), std::make_unique<::google::protobuf::Empty>(), std::make_unique<ScenePlayerImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[SceneScenePlayerEnterSceneMessageId] = RpcService{"SceneScenePlayer", "EnterScene", std::make_unique<::GsEnterSceneRequest>(), std::make_unique<::google::protobuf::Empty>(), std::make_unique<SceneScenePlayerImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[SceneScenePlayerLeaveSceneMessageId] = RpcService{"SceneScenePlayer", "LeaveScene", std::make_unique<::GsLeaveSceneRequest>(), std::make_unique<::google::protobuf::Empty>(), std::make_unique<SceneScenePlayerImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[SceneScenePlayerEnterSceneS2CMessageId] = RpcService{"SceneScenePlayer", "EnterSceneS2C", std::make_unique<::EnterSceneS2CRequest>(), std::make_unique<::EnterScenerS2CResponse>(), std::make_unique<SceneScenePlayerImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[SceneSceneTestMessageId] = RpcService{"SceneScene", "Test", std::make_unique<::GameSceneTest>(), std::make_unique<::Empty>(), std::make_unique<SceneSceneImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[SceneSceneClientPlayerEnterSceneMessageId] = RpcService{"SceneSceneClientPlayer", "EnterScene", std::make_unique<::EnterSceneC2SRequest>(), std::make_unique<::EnterSceneC2SResponse>(), std::make_unique<SceneSceneClientPlayerImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[SceneSceneClientPlayerNotifyEnterSceneMessageId] = RpcService{"SceneSceneClientPlayer", "NotifyEnterScene", std::make_unique<::EnterSceneS2C>(), std::make_unique<::Empty>(), std::make_unique<SceneSceneClientPlayerImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[SceneSceneClientPlayerSceneInfoC2SMessageId] = RpcService{"SceneSceneClientPlayer", "SceneInfoC2S", std::make_unique<::SceneInfoRequest>(), std::make_unique<::Empty>(), std::make_unique<SceneSceneClientPlayerImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[SceneSceneClientPlayerNotifySceneInfoMessageId] = RpcService{"SceneSceneClientPlayer", "NotifySceneInfo", std::make_unique<::SceneInfoS2C>(), std::make_unique<::Empty>(), std::make_unique<SceneSceneClientPlayerImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[SceneSceneClientPlayerNotifyActorCreateMessageId] = RpcService{"SceneSceneClientPlayer", "NotifyActorCreate", std::make_unique<::ActorCreateS2C>(), std::make_unique<::Empty>(), std::make_unique<SceneSceneClientPlayerImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[SceneSceneClientPlayerNotifyActorDestroyMessageId] = RpcService{"SceneSceneClientPlayer", "NotifyActorDestroy", std::make_unique<::ActorDestroyS2C>(), std::make_unique<::Empty>(), std::make_unique<SceneSceneClientPlayerImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[SceneSceneClientPlayerNotifyActorListCreateMessageId] = RpcService{"SceneSceneClientPlayer", "NotifyActorListCreate", std::make_unique<::ActorListCreateS2C>(), std::make_unique<::Empty>(), std::make_unique<SceneSceneClientPlayerImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[SceneSceneClientPlayerNotifyActorListDestroyMessageId] = RpcService{"SceneSceneClientPlayer", "NotifyActorListDestroy", std::make_unique<::ActorListDestroyS2C>(), std::make_unique<::Empty>(), std::make_unique<SceneSceneClientPlayerImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[SceneSkillClientPlayerReleaseSkillMessageId] = RpcService{"SceneSkillClientPlayer", "ReleaseSkill", std::make_unique<::ReleaseSkillSkillRequest>(), std::make_unique<::ReleaseSkillSkillResponse>(), std::make_unique<SceneSkillClientPlayerImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[SceneSkillClientPlayerNotifySkillUsedMessageId] = RpcService{"SceneSkillClientPlayer", "NotifySkillUsed", std::make_unique<::SkillUsedS2C>(), std::make_unique<::Empty>(), std::make_unique<SceneSkillClientPlayerImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[SceneSkillClientPlayerNotifySkillInterruptedMessageId] = RpcService{"SceneSkillClientPlayer", "NotifySkillInterrupted", std::make_unique<::SkillInterruptedS2C>(), std::make_unique<::Empty>(), std::make_unique<SceneSkillClientPlayerImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[SceneSkillClientPlayerGetSkillListMessageId] = RpcService{"SceneSkillClientPlayer", "GetSkillList", std::make_unique<::GetSkillListRequest>(), std::make_unique<::GetSkillListResponse>(), std::make_unique<SceneSkillClientPlayerImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[ScenePlayerSyncSyncBaseAttributeMessageId] = RpcService{"ScenePlayerSync", "SyncBaseAttribute", std::make_unique<::ActorBaseAttributesS2C>(), std::make_unique<::Empty>(), std::make_unique<ScenePlayerSyncImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[ScenePlayerSyncSyncAttribute2FramesMessageId] = RpcService{"ScenePlayerSync", "SyncAttribute2Frames", std::make_unique<::AttributeDelta2FramesS2C>(), std::make_unique<::Empty>(), std::make_unique<ScenePlayerSyncImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[ScenePlayerSyncSyncAttribute5FramesMessageId] = RpcService{"ScenePlayerSync", "SyncAttribute5Frames", std::make_unique<::AttributeDelta5FramesS2C>(), std::make_unique<::Empty>(), std::make_unique<ScenePlayerSyncImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[ScenePlayerSyncSyncAttribute10FramesMessageId] = RpcService{"ScenePlayerSync", "SyncAttribute10Frames", std::make_unique<::AttributeDelta10FramesS2C>(), std::make_unique<::Empty>(), std::make_unique<ScenePlayerSyncImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[ScenePlayerSyncSyncAttribute30FramesMessageId] = RpcService{"ScenePlayerSync", "SyncAttribute30Frames", std::make_unique<::AttributeDelta30FramesS2C>(), std::make_unique<::Empty>(), std::make_unique<ScenePlayerSyncImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[ScenePlayerSyncSyncAttribute60FramesMessageId] = RpcService{"ScenePlayerSync", "SyncAttribute60Frames", std::make_unique<::AttributeDelta60FramesS2C>(), std::make_unique<::Empty>(), std::make_unique<ScenePlayerSyncImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[ScenePlayerEnterGameNodeMessageId] = RpcService{"Scene", "PlayerEnterGameNode", std::make_unique<::PlayerEnterGameNodeRequest>(), std::make_unique<::Empty>(), std::make_unique<SceneImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[SceneSendMessageToPlayerMessageId] = RpcService{"Scene", "SendMessageToPlayer", std::make_unique<::NodeRouteMessageRequest>(), std::make_unique<::NodeRouteMessageResponse>(), std::make_unique<SceneImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[SceneProcessClientPlayerMessageMessageId] = RpcService{"Scene", "ProcessClientPlayerMessage", std::make_unique<::ProcessClientPlayerMessageRequest>(), std::make_unique<::ProcessClientPlayerMessageResponse>(), std::make_unique<SceneImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[SceneCentreSendToPlayerViaGameNodeMessageId] = RpcService{"Scene", "CentreSendToPlayerViaGameNode", std::make_unique<::NodeRouteMessageRequest>(), std::make_unique<::Empty>(), std::make_unique<SceneImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[SceneInvokePlayerServiceMessageId] = RpcService{"Scene", "InvokePlayerService", std::make_unique<::NodeRouteMessageRequest>(), std::make_unique<::NodeRouteMessageResponse>(), std::make_unique<SceneImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[SceneRouteNodeStringMsgMessageId] = RpcService{"Scene", "RouteNodeStringMsg", std::make_unique<::RouteMessageRequest>(), std::make_unique<::RouteMessageResponse>(), std::make_unique<SceneImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[SceneRoutePlayerStringMsgMessageId] = RpcService{"Scene", "RoutePlayerStringMsg", std::make_unique<::RoutePlayerMessageRequest>(), std::make_unique<::RoutePlayerMessageResponse>(), std::make_unique<SceneImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[SceneUpdateSessionDetailMessageId] = RpcService{"Scene", "UpdateSessionDetail", std::make_unique<::RegisterPlayerSessionRequest>(), std::make_unique<::Empty>(), std::make_unique<SceneImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[SceneEnterSceneMessageId] = RpcService{"Scene", "EnterScene", std::make_unique<::Centre2GsEnterSceneRequest>(), std::make_unique<::Empty>(), std::make_unique<SceneImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[SceneCreateSceneMessageId] = RpcService{"Scene", "CreateScene", std::make_unique<::CreateSceneRequest>(), std::make_unique<::CreateSceneResponse>(), std::make_unique<SceneImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[SceneNodeHandshakeMessageId] = RpcService{"Scene", "NodeHandshake", std::make_unique<::NodeHandshakeRequest>(), std::make_unique<::NodeHandshakeResponse>(), std::make_unique<SceneImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[SceneManagerCreateSceneMessageId] = RpcService{"SceneManager", "CreateScene", std::make_unique<::scene_manager::CreateSceneRequest>(), std::make_unique<::scene_manager::CreateSceneResponse>(), nullptr, 0, eNodeType::SceneManagerNodeService, scene_manager::SendSceneManagerCreateScene};
    gRpcServiceRegistry[SceneManagerDestroySceneMessageId] = RpcService{"SceneManager", "DestroyScene", std::make_unique<::scene_manager::DestroySceneRequest>(), std::make_unique<::Empty>(), nullptr, 0, eNodeType::SceneManagerNodeService, scene_manager::SendSceneManagerDestroyScene};
    gRpcServiceRegistry[SceneManagerEnterSceneMessageId] = RpcService{"SceneManager", "EnterScene", std::make_unique<::scene_manager::EnterSceneRequest>(), std::make_unique<::scene_manager::EnterSceneResponse>(), nullptr, 0, eNodeType::SceneManagerNodeService, scene_manager::SendSceneManagerEnterScene};
    gRpcServiceRegistry[SceneManagerLeaveSceneMessageId] = RpcService{"SceneManager", "LeaveScene", std::make_unique<::scene_manager::LeaveSceneRequest>(), std::make_unique<::Empty>(), nullptr, 0, eNodeType::SceneManagerNodeService, scene_manager::SendSceneManagerLeaveScene};

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
	gEventPrototypes[AcceptMissionEventEventId] = std::make_unique<AcceptMissionEvent>();
	gEventPrototypes[AfterEnterSceneEventId] = std::make_unique<AfterEnterScene>();
	gEventPrototypes[AfterLeaveSceneEventId] = std::make_unique<AfterLeaveScene>();
	gEventPrototypes[BeKillEventEventId] = std::make_unique<BeKillEvent>();
	gEventPrototypes[BeforeEnterSceneEventId] = std::make_unique<BeforeEnterScene>();
	gEventPrototypes[BeforeLeaveSceneEventId] = std::make_unique<BeforeLeaveScene>();
	gEventPrototypes[BuffTestEvetEventId] = std::make_unique<BuffTestEvet>();
	gEventPrototypes[CombatStateAddedPbEventEventId] = std::make_unique<CombatStateAddedPbEvent>();
	gEventPrototypes[CombatStateRemovedPbEventEventId] = std::make_unique<CombatStateRemovedPbEvent>();
	gEventPrototypes[ConnectToNodePbEventEventId] = std::make_unique<ConnectToNodePbEvent>();
	gEventPrototypes[ContractsKafkaBindSessionEventEventId] = std::make_unique<contracts::kafka::BindSessionEvent>();
	gEventPrototypes[ContractsKafkaKickPlayerEventEventId] = std::make_unique<contracts::kafka::KickPlayerEvent>();
	gEventPrototypes[ContractsKafkaPlayerDisconnectedEventEventId] = std::make_unique<contracts::kafka::PlayerDisconnectedEvent>();
	gEventPrototypes[ContractsKafkaPlayerLeaseExpiredEventEventId] = std::make_unique<contracts::kafka::PlayerLeaseExpiredEvent>();
	gEventPrototypes[ContractsKafkaPlayerLifecycleCommandEventId] = std::make_unique<contracts::kafka::PlayerLifecycleCommand>();
	gEventPrototypes[ContractsKafkaRoutePlayerEventEventId] = std::make_unique<contracts::kafka::RoutePlayerEvent>();
	gEventPrototypes[InitializeActorComponentsEventEventId] = std::make_unique<InitializeActorComponentsEvent>();
	gEventPrototypes[InitializeNpcComponentsEventEventId] = std::make_unique<InitializeNpcComponentsEvent>();
	gEventPrototypes[InitializePlayerComponentsEventEventId] = std::make_unique<InitializePlayerComponentsEvent>();
	gEventPrototypes[InterruptCurrentStatePbEventEventId] = std::make_unique<InterruptCurrentStatePbEvent>();
	gEventPrototypes[MissionConditionEventEventId] = std::make_unique<MissionConditionEvent>();
	gEventPrototypes[OnAcceptedMissionEventEventId] = std::make_unique<OnAcceptedMissionEvent>();
	gEventPrototypes[OnConnect2CentrePbEventEventId] = std::make_unique<OnConnect2CentrePbEvent>();
	gEventPrototypes[OnConnect2LoginEventId] = std::make_unique<OnConnect2Login>();
	gEventPrototypes[OnMissionAwardEventEventId] = std::make_unique<OnMissionAwardEvent>();
	gEventPrototypes[OnNodeAddPbEventEventId] = std::make_unique<OnNodeAddPbEvent>();
	gEventPrototypes[OnNodeConnectedPbEventEventId] = std::make_unique<OnNodeConnectedPbEvent>();
	gEventPrototypes[OnNodeRemovePbEventEventId] = std::make_unique<OnNodeRemovePbEvent>();
	gEventPrototypes[OnSceneCreatedEventId] = std::make_unique<OnSceneCreated>();
	gEventPrototypes[OnSceneDestroyedEventId] = std::make_unique<OnSceneDestroyed>();
	gEventPrototypes[OnServerStartEventId] = std::make_unique<OnServerStart>();
	gEventPrototypes[PlayerMigrationPbEventEventId] = std::make_unique<PlayerMigrationPbEvent>();
	gEventPrototypes[PlayerUpgradeEventEventId] = std::make_unique<PlayerUpgradeEvent>();
	gEventPrototypes[RegisterPlayerEventEventId] = std::make_unique<RegisterPlayerEvent>();
	gEventPrototypes[S2CEnterSceneEventId] = std::make_unique<S2CEnterScene>();
	gEventPrototypes[SkillExecutedEventEventId] = std::make_unique<SkillExecutedEvent>();
}

bool IsValidEventId(uint32_t eventId)
{
	return eventId < kMaxEventLen && gEventPrototypes[eventId] != nullptr;
}

MessageUniquePtr NewEventMessage(uint32_t eventId)
{
	if (eventId >= kMaxEventLen || !gEventPrototypes[eventId]) {
		return nullptr;
	}
	return MessageUniquePtr(gEventPrototypes[eventId]->New());
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
	switch (eventId) {
	case AcceptMissionEventEventId: dispatcher.enqueue(static_cast<const AcceptMissionEvent&>(message)); break;
	case AfterEnterSceneEventId: dispatcher.enqueue(static_cast<const AfterEnterScene&>(message)); break;
	case AfterLeaveSceneEventId: dispatcher.enqueue(static_cast<const AfterLeaveScene&>(message)); break;
	case BeKillEventEventId: dispatcher.enqueue(static_cast<const BeKillEvent&>(message)); break;
	case BeforeEnterSceneEventId: dispatcher.enqueue(static_cast<const BeforeEnterScene&>(message)); break;
	case BeforeLeaveSceneEventId: dispatcher.enqueue(static_cast<const BeforeLeaveScene&>(message)); break;
	case BuffTestEvetEventId: dispatcher.enqueue(static_cast<const BuffTestEvet&>(message)); break;
	case CombatStateAddedPbEventEventId: dispatcher.enqueue(static_cast<const CombatStateAddedPbEvent&>(message)); break;
	case CombatStateRemovedPbEventEventId: dispatcher.enqueue(static_cast<const CombatStateRemovedPbEvent&>(message)); break;
	case ConnectToNodePbEventEventId: dispatcher.enqueue(static_cast<const ConnectToNodePbEvent&>(message)); break;
	case ContractsKafkaBindSessionEventEventId: dispatcher.enqueue(static_cast<const contracts::kafka::BindSessionEvent&>(message)); break;
	case ContractsKafkaKickPlayerEventEventId: dispatcher.enqueue(static_cast<const contracts::kafka::KickPlayerEvent&>(message)); break;
	case ContractsKafkaPlayerDisconnectedEventEventId: dispatcher.enqueue(static_cast<const contracts::kafka::PlayerDisconnectedEvent&>(message)); break;
	case ContractsKafkaPlayerLeaseExpiredEventEventId: dispatcher.enqueue(static_cast<const contracts::kafka::PlayerLeaseExpiredEvent&>(message)); break;
	case ContractsKafkaPlayerLifecycleCommandEventId: dispatcher.enqueue(static_cast<const contracts::kafka::PlayerLifecycleCommand&>(message)); break;
	case ContractsKafkaRoutePlayerEventEventId: dispatcher.enqueue(static_cast<const contracts::kafka::RoutePlayerEvent&>(message)); break;
	case InitializeActorComponentsEventEventId: dispatcher.enqueue(static_cast<const InitializeActorComponentsEvent&>(message)); break;
	case InitializeNpcComponentsEventEventId: dispatcher.enqueue(static_cast<const InitializeNpcComponentsEvent&>(message)); break;
	case InitializePlayerComponentsEventEventId: dispatcher.enqueue(static_cast<const InitializePlayerComponentsEvent&>(message)); break;
	case InterruptCurrentStatePbEventEventId: dispatcher.enqueue(static_cast<const InterruptCurrentStatePbEvent&>(message)); break;
	case MissionConditionEventEventId: dispatcher.enqueue(static_cast<const MissionConditionEvent&>(message)); break;
	case OnAcceptedMissionEventEventId: dispatcher.enqueue(static_cast<const OnAcceptedMissionEvent&>(message)); break;
	case OnConnect2CentrePbEventEventId: dispatcher.enqueue(static_cast<const OnConnect2CentrePbEvent&>(message)); break;
	case OnConnect2LoginEventId: dispatcher.enqueue(static_cast<const OnConnect2Login&>(message)); break;
	case OnMissionAwardEventEventId: dispatcher.enqueue(static_cast<const OnMissionAwardEvent&>(message)); break;
	case OnNodeAddPbEventEventId: dispatcher.enqueue(static_cast<const OnNodeAddPbEvent&>(message)); break;
	case OnNodeConnectedPbEventEventId: dispatcher.enqueue(static_cast<const OnNodeConnectedPbEvent&>(message)); break;
	case OnNodeRemovePbEventEventId: dispatcher.enqueue(static_cast<const OnNodeRemovePbEvent&>(message)); break;
	case OnSceneCreatedEventId: dispatcher.enqueue(static_cast<const OnSceneCreated&>(message)); break;
	case OnSceneDestroyedEventId: dispatcher.enqueue(static_cast<const OnSceneDestroyed&>(message)); break;
	case OnServerStartEventId: dispatcher.enqueue(static_cast<const OnServerStart&>(message)); break;
	case PlayerMigrationPbEventEventId: dispatcher.enqueue(static_cast<const PlayerMigrationPbEvent&>(message)); break;
	case PlayerUpgradeEventEventId: dispatcher.enqueue(static_cast<const PlayerUpgradeEvent&>(message)); break;
	case RegisterPlayerEventEventId: dispatcher.enqueue(static_cast<const RegisterPlayerEvent&>(message)); break;
	case S2CEnterSceneEventId: dispatcher.enqueue(static_cast<const S2CEnterScene&>(message)); break;
	case SkillExecutedEventEventId: dispatcher.enqueue(static_cast<const SkillExecutedEvent&>(message)); break;
	default:
		break;
	}
}
