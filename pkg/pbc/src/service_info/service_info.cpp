#include <array>
#include "service_info.h"
#include "proto/common/node.pb.h"  

#include "proto/db/db_service.grpc.pb.h"
#include "proto/centre/centre_client_player.pb.h"
#include "proto/centre/centre_player_scene.pb.h"
#include "proto/centre/centre_player.pb.h"
#include "proto/centre/centre_scene.pb.h"
#include "proto/centre/centre_service.pb.h"
#include "proto/scene/player_scene.pb.h"
#include "proto/scene/player_skill.pb.h"
#include "proto/scene/player_state_attribute_sync.pb.h"
#include "proto/scene/game_client_player.pb.h"
#include "proto/scene/game_player_scene.pb.h"
#include "proto/scene/game_player.pb.h"
#include "proto/scene/game_scene.pb.h"
#include "proto/scene/game_service.pb.h"
#include "proto/gate/gate_service.pb.h"
#include "proto/etcd/etcd.grpc.pb.h"
#include "proto/etcd/etcd.grpc.pb.h"
#include "proto/login/login_service.grpc.pb.h"
#include "proto/db/db_service.grpc.pb.h"
#include "proto/etcd/etcd.grpc.pb.h"
#include "db_service_service_info.h"
#include "centre_client_player_service_info.h"
#include "centre_player_scene_service_info.h"
#include "centre_player_service_info.h"
#include "centre_scene_service_info.h"
#include "centre_service_service_info.h"
#include "player_scene_service_info.h"
#include "player_skill_service_info.h"
#include "player_state_attribute_sync_service_info.h"
#include "game_client_player_service_info.h"
#include "game_player_scene_service_info.h"
#include "game_player_service_info.h"
#include "game_scene_service_info.h"
#include "game_service_service_info.h"
#include "gate_service_service_info.h"
#include "etcd_service_info.h"
#include "etcd_service_info.h"
#include "login_service_service_info.h"
#include "db_service_service_info.h"
#include "etcd_service_info.h"


class CentreClientPlayerCommonServiceImpl final : public CentreClientPlayerCommonService {};
class CentrePlayerSceneServiceImpl final : public CentrePlayerSceneService {};
class CentrePlayerServiceImpl final : public CentrePlayerService {};
class CentreSceneServiceImpl final : public CentreSceneService {};
class CentreServiceImpl final : public CentreService {};
class ClientPlayerSceneServiceImpl final : public ClientPlayerSceneService {};
class ClientPlayerSkillServiceImpl final : public ClientPlayerSkillService {};
class EntitySyncServiceImpl final : public EntitySyncService {};
class GameClientPlayerCommonServiceImpl final : public GameClientPlayerCommonService {};
class GamePlayerSceneServiceImpl final : public GamePlayerSceneService {};
class GamePlayerServiceImpl final : public GamePlayerService {};
class GameSceneServiceImpl final : public GameSceneService {};
class GameServiceImpl final : public GameService {};
class GateServiceImpl final : public GateService {};

std::unordered_set<uint32_t> gClientToServerMessageId;
std::array<RpcService, 1> gMessageInfo;

void InitMessageInfo()
{
    gMessageInfo[AccountDBServiceLoad2RedisMessageId] = RpcService{"AccountDBService", "Load2Redis", "::LoadAccountRequest", "::LoadAccountResponse", nullptr, 0, eNodeType::DbNodeService};
    gMessageInfo[AccountDBServiceSave2RedisMessageId] = RpcService{"AccountDBService", "Save2Redis", "::SaveAccountRequest", "::SaveAccountResponse", nullptr, 0, eNodeType::DbNodeService};
    gMessageInfo[CentreClientPlayerCommonServiceSendTipToClientMessageId] = RpcService{"CentreClientPlayerCommonService", "SendTipToClient", "::TipInfoMessage", "::Empty", std::make_unique_for_overwrite<CentreClientPlayerCommonServiceImpl>(), 0, eNodeType::CentreNodeService};
    gMessageInfo[CentreClientPlayerCommonServiceKickPlayerMessageId] = RpcService{"CentreClientPlayerCommonService", "KickPlayer", "::CentreKickPlayerRequest", "::Empty", std::make_unique_for_overwrite<CentreClientPlayerCommonServiceImpl>(), 0, eNodeType::CentreNodeService};
    gMessageInfo[CentrePlayerSceneServiceEnterSceneMessageId] = RpcService{"CentrePlayerSceneService", "EnterScene", "::CentreEnterSceneRequest", "::google::protobuf::Empty", std::make_unique_for_overwrite<CentrePlayerSceneServiceImpl>(), 0, eNodeType::CentreNodeService};
    gMessageInfo[CentrePlayerSceneServiceLeaveSceneMessageId] = RpcService{"CentrePlayerSceneService", "LeaveScene", "::CentreLeaveSceneRequest", "::google::protobuf::Empty", std::make_unique_for_overwrite<CentrePlayerSceneServiceImpl>(), 0, eNodeType::CentreNodeService};
    gMessageInfo[CentrePlayerSceneServiceLeaveSceneAsyncSavePlayerCompleteMessageId] = RpcService{"CentrePlayerSceneService", "LeaveSceneAsyncSavePlayerComplete", "::CentreLeaveSceneAsyncSavePlayerCompleteRequest", "::google::protobuf::Empty", std::make_unique_for_overwrite<CentrePlayerSceneServiceImpl>(), 0, eNodeType::CentreNodeService};
    gMessageInfo[CentrePlayerSceneServiceSceneInfoC2SMessageId] = RpcService{"CentrePlayerSceneService", "SceneInfoC2S", "::CentreSceneInfoRequest", "::google::protobuf::Empty", std::make_unique_for_overwrite<CentrePlayerSceneServiceImpl>(), 0, eNodeType::CentreNodeService};
    gMessageInfo[CentrePlayerServiceTestMessageId] = RpcService{"CentrePlayerService", "Test", "::google::protobuf::Empty", "::google::protobuf::Empty", std::make_unique_for_overwrite<CentrePlayerServiceImpl>(), 0, eNodeType::CentreNodeService};
    gMessageInfo[CentreSceneServiceRegisterSceneMessageId] = RpcService{"CentreSceneService", "RegisterScene", "::RegisterSceneRequest", "::RegisterSceneResponse", std::make_unique_for_overwrite<CentreSceneServiceImpl>(), 0, eNodeType::CentreNodeService};
    gMessageInfo[CentreSceneServiceUnRegisterSceneMessageId] = RpcService{"CentreSceneService", "UnRegisterScene", "::UnRegisterSceneRequest", "::Empty", std::make_unique_for_overwrite<CentreSceneServiceImpl>(), 0, eNodeType::CentreNodeService};
    gMessageInfo[CentreServiceGatePlayerServiceMessageId] = RpcService{"CentreService", "GatePlayerService", "::GateClientMessageRequest", "::Empty", std::make_unique_for_overwrite<CentreServiceImpl>(), 0, eNodeType::CentreNodeService};
    gMessageInfo[CentreServiceGateSessionDisconnectMessageId] = RpcService{"CentreService", "GateSessionDisconnect", "::GateSessionDisconnectRequest", "::Empty", std::make_unique_for_overwrite<CentreServiceImpl>(), 0, eNodeType::CentreNodeService};
    gMessageInfo[CentreServiceLoginNodeAccountLoginMessageId] = RpcService{"CentreService", "LoginNodeAccountLogin", "::LoginRequest", "::LoginResponse", std::make_unique_for_overwrite<CentreServiceImpl>(), 0, eNodeType::CentreNodeService};
    gMessageInfo[CentreServiceLoginNodeEnterGameMessageId] = RpcService{"CentreService", "LoginNodeEnterGame", "::CentrePlayerGameNodeEntryRequest", "::Empty", std::make_unique_for_overwrite<CentreServiceImpl>(), 0, eNodeType::CentreNodeService};
    gMessageInfo[CentreServiceLoginNodeLeaveGameMessageId] = RpcService{"CentreService", "LoginNodeLeaveGame", "::LoginNodeLeaveGameRequest", "::Empty", std::make_unique_for_overwrite<CentreServiceImpl>(), 0, eNodeType::CentreNodeService};
    gMessageInfo[CentreServiceLoginNodeSessionDisconnectMessageId] = RpcService{"CentreService", "LoginNodeSessionDisconnect", "::GateSessionDisconnectRequest", "::Empty", std::make_unique_for_overwrite<CentreServiceImpl>(), 0, eNodeType::CentreNodeService};
    gMessageInfo[CentreServicePlayerServiceMessageId] = RpcService{"CentreService", "PlayerService", "::NodeRouteMessageRequest", "::NodeRouteMessageResponse", std::make_unique_for_overwrite<CentreServiceImpl>(), 0, eNodeType::CentreNodeService};
    gMessageInfo[CentreServiceEnterGsSucceedMessageId] = RpcService{"CentreService", "EnterGsSucceed", "::EnterGameNodeSuccessRequest", "::Empty", std::make_unique_for_overwrite<CentreServiceImpl>(), 0, eNodeType::CentreNodeService};
    gMessageInfo[CentreServiceRouteNodeStringMsgMessageId] = RpcService{"CentreService", "RouteNodeStringMsg", "::RouteMessageRequest", "::RouteMessageResponse", std::make_unique_for_overwrite<CentreServiceImpl>(), 0, eNodeType::CentreNodeService};
    gMessageInfo[CentreServiceRoutePlayerStringMsgMessageId] = RpcService{"CentreService", "RoutePlayerStringMsg", "::RoutePlayerMessageRequest", "::RoutePlayerMessageResponse", std::make_unique_for_overwrite<CentreServiceImpl>(), 0, eNodeType::CentreNodeService};
    gMessageInfo[CentreServiceInitSceneNodeMessageId] = RpcService{"CentreService", "InitSceneNode", "::InitSceneNodeRequest", "::Empty", std::make_unique_for_overwrite<CentreServiceImpl>(), 0, eNodeType::CentreNodeService};
    gMessageInfo[CentreServiceRegisterNodeSessionMessageId] = RpcService{"CentreService", "RegisterNodeSession", "::RegisterNodeSessionRequest", "::RegisterNodeSessionResponse", std::make_unique_for_overwrite<CentreServiceImpl>(), 0, eNodeType::CentreNodeService};
    gMessageInfo[ClientPlayerSceneServiceEnterSceneMessageId] = RpcService{"ClientPlayerSceneService", "EnterScene", "::EnterSceneC2SRequest", "::EnterSceneC2SResponse", std::make_unique_for_overwrite<ClientPlayerSceneServiceImpl>(), 0, eNodeType::SceneNodeService};
    gMessageInfo[ClientPlayerSceneServiceNotifyEnterSceneMessageId] = RpcService{"ClientPlayerSceneService", "NotifyEnterScene", "::EnterSceneS2C", "::Empty", std::make_unique_for_overwrite<ClientPlayerSceneServiceImpl>(), 0, eNodeType::SceneNodeService};
    gMessageInfo[ClientPlayerSceneServiceSceneInfoC2SMessageId] = RpcService{"ClientPlayerSceneService", "SceneInfoC2S", "::SceneInfoRequest", "::Empty", std::make_unique_for_overwrite<ClientPlayerSceneServiceImpl>(), 0, eNodeType::SceneNodeService};
    gMessageInfo[ClientPlayerSceneServiceNotifySceneInfoMessageId] = RpcService{"ClientPlayerSceneService", "NotifySceneInfo", "::SceneInfoS2C", "::Empty", std::make_unique_for_overwrite<ClientPlayerSceneServiceImpl>(), 0, eNodeType::SceneNodeService};
    gMessageInfo[ClientPlayerSceneServiceNotifyActorCreateMessageId] = RpcService{"ClientPlayerSceneService", "NotifyActorCreate", "::ActorCreateS2C", "::Empty", std::make_unique_for_overwrite<ClientPlayerSceneServiceImpl>(), 0, eNodeType::SceneNodeService};
    gMessageInfo[ClientPlayerSceneServiceNotifyActorDestroyMessageId] = RpcService{"ClientPlayerSceneService", "NotifyActorDestroy", "::ActorDestroyS2C", "::Empty", std::make_unique_for_overwrite<ClientPlayerSceneServiceImpl>(), 0, eNodeType::SceneNodeService};
    gMessageInfo[ClientPlayerSceneServiceNotifyActorListCreateMessageId] = RpcService{"ClientPlayerSceneService", "NotifyActorListCreate", "::ActorListCreateS2C", "::Empty", std::make_unique_for_overwrite<ClientPlayerSceneServiceImpl>(), 0, eNodeType::SceneNodeService};
    gMessageInfo[ClientPlayerSceneServiceNotifyActorListDestroyMessageId] = RpcService{"ClientPlayerSceneService", "NotifyActorListDestroy", "::ActorListDestroyS2C", "::Empty", std::make_unique_for_overwrite<ClientPlayerSceneServiceImpl>(), 0, eNodeType::SceneNodeService};
    gMessageInfo[ClientPlayerSkillServiceReleaseSkillMessageId] = RpcService{"ClientPlayerSkillService", "ReleaseSkill", "::ReleaseSkillSkillRequest", "::ReleaseSkillSkillResponse", std::make_unique_for_overwrite<ClientPlayerSkillServiceImpl>(), 0, eNodeType::SceneNodeService};
    gMessageInfo[ClientPlayerSkillServiceNotifySkillUsedMessageId] = RpcService{"ClientPlayerSkillService", "NotifySkillUsed", "::SkillUsedS2C", "::Empty", std::make_unique_for_overwrite<ClientPlayerSkillServiceImpl>(), 0, eNodeType::SceneNodeService};
    gMessageInfo[ClientPlayerSkillServiceNotifySkillInterruptedMessageId] = RpcService{"ClientPlayerSkillService", "NotifySkillInterrupted", "::SkillInterruptedS2C", "::Empty", std::make_unique_for_overwrite<ClientPlayerSkillServiceImpl>(), 0, eNodeType::SceneNodeService};
    gMessageInfo[ClientPlayerSkillServiceGetSkillListMessageId] = RpcService{"ClientPlayerSkillService", "GetSkillList", "::GetSkillListRequest", "::GetSkillListResponse", std::make_unique_for_overwrite<ClientPlayerSkillServiceImpl>(), 0, eNodeType::SceneNodeService};
    gMessageInfo[EntitySyncServiceSyncBaseAttributeMessageId] = RpcService{"EntitySyncService", "SyncBaseAttribute", "::BaseAttributeSyncDataS2C", "::Empty", std::make_unique_for_overwrite<EntitySyncServiceImpl>(), 0, eNodeType::SceneNodeService};
    gMessageInfo[EntitySyncServiceSyncAttribute2FramesMessageId] = RpcService{"EntitySyncService", "SyncAttribute2Frames", "::AttributeDelta2FramesS2C", "::Empty", std::make_unique_for_overwrite<EntitySyncServiceImpl>(), 0, eNodeType::SceneNodeService};
    gMessageInfo[EntitySyncServiceSyncAttribute5FramesMessageId] = RpcService{"EntitySyncService", "SyncAttribute5Frames", "::AttributeDelta5FramesS2C", "::Empty", std::make_unique_for_overwrite<EntitySyncServiceImpl>(), 0, eNodeType::SceneNodeService};
    gMessageInfo[EntitySyncServiceSyncAttribute10FramesMessageId] = RpcService{"EntitySyncService", "SyncAttribute10Frames", "::AttributeDelta10FramesS2C", "::Empty", std::make_unique_for_overwrite<EntitySyncServiceImpl>(), 0, eNodeType::SceneNodeService};
    gMessageInfo[EntitySyncServiceSyncAttribute30FramesMessageId] = RpcService{"EntitySyncService", "SyncAttribute30Frames", "::AttributeDelta30FramesS2C", "::Empty", std::make_unique_for_overwrite<EntitySyncServiceImpl>(), 0, eNodeType::SceneNodeService};
    gMessageInfo[EntitySyncServiceSyncAttribute60FramesMessageId] = RpcService{"EntitySyncService", "SyncAttribute60Frames", "::AttributeDelta60FramesS2C", "::Empty", std::make_unique_for_overwrite<EntitySyncServiceImpl>(), 0, eNodeType::SceneNodeService};
    gMessageInfo[GameClientPlayerCommonServiceSendTipToClientMessageId] = RpcService{"GameClientPlayerCommonService", "SendTipToClient", "::TipInfoMessage", "::Empty", std::make_unique_for_overwrite<GameClientPlayerCommonServiceImpl>(), 0, eNodeType::SceneNodeService};
    gMessageInfo[GameClientPlayerCommonServiceKickPlayerMessageId] = RpcService{"GameClientPlayerCommonService", "KickPlayer", "::GameKickPlayerRequest", "::Empty", std::make_unique_for_overwrite<GameClientPlayerCommonServiceImpl>(), 0, eNodeType::SceneNodeService};
    gMessageInfo[GamePlayerSceneServiceEnterSceneMessageId] = RpcService{"GamePlayerSceneService", "EnterScene", "::GsEnterSceneRequest", "::google::protobuf::Empty", std::make_unique_for_overwrite<GamePlayerSceneServiceImpl>(), 0, eNodeType::SceneNodeService};
    gMessageInfo[GamePlayerSceneServiceLeaveSceneMessageId] = RpcService{"GamePlayerSceneService", "LeaveScene", "::GsLeaveSceneRequest", "::google::protobuf::Empty", std::make_unique_for_overwrite<GamePlayerSceneServiceImpl>(), 0, eNodeType::SceneNodeService};
    gMessageInfo[GamePlayerSceneServiceEnterSceneS2CMessageId] = RpcService{"GamePlayerSceneService", "EnterSceneS2C", "::EnterSceneS2CRequest", "::EnterScenerS2CResponse", std::make_unique_for_overwrite<GamePlayerSceneServiceImpl>(), 0, eNodeType::SceneNodeService};
    gMessageInfo[GamePlayerServiceCentre2GsLoginMessageId] = RpcService{"GamePlayerService", "Centre2GsLogin", "::Centre2GsLoginRequest", "::google::protobuf::Empty", std::make_unique_for_overwrite<GamePlayerServiceImpl>(), 0, eNodeType::SceneNodeService};
    gMessageInfo[GamePlayerServiceExitGameMessageId] = RpcService{"GamePlayerService", "ExitGame", "::GameNodeExitGameRequest", "::google::protobuf::Empty", std::make_unique_for_overwrite<GamePlayerServiceImpl>(), 0, eNodeType::SceneNodeService};
    gMessageInfo[GameSceneServiceTestMessageId] = RpcService{"GameSceneService", "Test", "::GameSceneTest", "::Empty", std::make_unique_for_overwrite<GameSceneServiceImpl>(), 0, eNodeType::SceneNodeService};
    gMessageInfo[GameServicePlayerEnterGameNodeMessageId] = RpcService{"GameService", "PlayerEnterGameNode", "::PlayerEnterGameNodeRequest", "::Empty", std::make_unique_for_overwrite<GameServiceImpl>(), 0, eNodeType::SceneNodeService};
    gMessageInfo[GameServiceSendMessageToPlayerMessageId] = RpcService{"GameService", "SendMessageToPlayer", "::NodeRouteMessageRequest", "::NodeRouteMessageResponse", std::make_unique_for_overwrite<GameServiceImpl>(), 0, eNodeType::SceneNodeService};
    gMessageInfo[GameServiceClientSendMessageToPlayerMessageId] = RpcService{"GameService", "ClientSendMessageToPlayer", "::ClientSendMessageToPlayerRequest", "::ClientSendMessageToPlayerResponse", std::make_unique_for_overwrite<GameServiceImpl>(), 0, eNodeType::SceneNodeService};
    gMessageInfo[GameServiceCentreSendToPlayerViaGameNodeMessageId] = RpcService{"GameService", "CentreSendToPlayerViaGameNode", "::NodeRouteMessageRequest", "::Empty", std::make_unique_for_overwrite<GameServiceImpl>(), 0, eNodeType::SceneNodeService};
    gMessageInfo[GameServiceInvokePlayerServiceMessageId] = RpcService{"GameService", "InvokePlayerService", "::NodeRouteMessageRequest", "::NodeRouteMessageResponse", std::make_unique_for_overwrite<GameServiceImpl>(), 0, eNodeType::SceneNodeService};
    gMessageInfo[GameServiceRouteNodeStringMsgMessageId] = RpcService{"GameService", "RouteNodeStringMsg", "::RouteMessageRequest", "::RouteMessageResponse", std::make_unique_for_overwrite<GameServiceImpl>(), 0, eNodeType::SceneNodeService};
    gMessageInfo[GameServiceRoutePlayerStringMsgMessageId] = RpcService{"GameService", "RoutePlayerStringMsg", "::RoutePlayerMessageRequest", "::RoutePlayerMessageResponse", std::make_unique_for_overwrite<GameServiceImpl>(), 0, eNodeType::SceneNodeService};
    gMessageInfo[GameServiceUpdateSessionDetailMessageId] = RpcService{"GameService", "UpdateSessionDetail", "::RegisterPlayerSessionRequest", "::Empty", std::make_unique_for_overwrite<GameServiceImpl>(), 0, eNodeType::SceneNodeService};
    gMessageInfo[GameServiceEnterSceneMessageId] = RpcService{"GameService", "EnterScene", "::Centre2GsEnterSceneRequest", "::Empty", std::make_unique_for_overwrite<GameServiceImpl>(), 0, eNodeType::SceneNodeService};
    gMessageInfo[GameServiceCreateSceneMessageId] = RpcService{"GameService", "CreateScene", "::CreateSceneRequest", "::CreateSceneResponse", std::make_unique_for_overwrite<GameServiceImpl>(), 0, eNodeType::SceneNodeService};
    gMessageInfo[GameServiceRegisterNodeSessionMessageId] = RpcService{"GameService", "RegisterNodeSession", "::RegisterNodeSessionRequest", "::RegisterNodeSessionResponse", std::make_unique_for_overwrite<GameServiceImpl>(), 0, eNodeType::SceneNodeService};
    gMessageInfo[GateServicePlayerEnterGameNodeMessageId] = RpcService{"GateService", "PlayerEnterGameNode", "::RegisterGameNodeSessionRequest", "::RegisterGameNodeSessionResponse", std::make_unique_for_overwrite<GateServiceImpl>(), 0, eNodeType::GateNodeService};
    gMessageInfo[GateServiceSendMessageToPlayerMessageId] = RpcService{"GateService", "SendMessageToPlayer", "::NodeRouteMessageRequest", "::Empty", std::make_unique_for_overwrite<GateServiceImpl>(), 0, eNodeType::GateNodeService};
    gMessageInfo[GateServiceKickSessionByCentreMessageId] = RpcService{"GateService", "KickSessionByCentre", "::KickSessionRequest", "::Empty", std::make_unique_for_overwrite<GateServiceImpl>(), 0, eNodeType::GateNodeService};
    gMessageInfo[GateServiceRouteNodeMessageMessageId] = RpcService{"GateService", "RouteNodeMessage", "::RouteMessageRequest", "::RouteMessageResponse", std::make_unique_for_overwrite<GateServiceImpl>(), 0, eNodeType::GateNodeService};
    gMessageInfo[GateServiceRoutePlayerMessageMessageId] = RpcService{"GateService", "RoutePlayerMessage", "::RoutePlayerMessageRequest", "::RoutePlayerMessageResponse", std::make_unique_for_overwrite<GateServiceImpl>(), 0, eNodeType::GateNodeService};
    gMessageInfo[GateServiceBroadcastToPlayersMessageId] = RpcService{"GateService", "BroadcastToPlayers", "::BroadcastToPlayersRequest", "::Empty", std::make_unique_for_overwrite<GateServiceImpl>(), 0, eNodeType::GateNodeService};
    gMessageInfo[GateServiceRegisterNodeSessionMessageId] = RpcService{"GateService", "RegisterNodeSession", "::RegisterNodeSessionRequest", "::RegisterNodeSessionResponse", std::make_unique_for_overwrite<GateServiceImpl>(), 0, eNodeType::GateNodeService};
    gMessageInfo[KVRangeMessageId] = RpcService{"KV", "Range", "::etcdserverpb::RangeRequest", "::etcdserverpb::RangeResponse", nullptr, 0, eNodeType::EtcdNodeService};
    gMessageInfo[KVPutMessageId] = RpcService{"KV", "Put", "::etcdserverpb::PutRequest", "::etcdserverpb::PutResponse", nullptr, 0, eNodeType::EtcdNodeService};
    gMessageInfo[KVDeleteRangeMessageId] = RpcService{"KV", "DeleteRange", "::etcdserverpb::DeleteRangeRequest", "::etcdserverpb::DeleteRangeResponse", nullptr, 0, eNodeType::EtcdNodeService};
    gMessageInfo[KVTxnMessageId] = RpcService{"KV", "Txn", "::etcdserverpb::TxnRequest", "::etcdserverpb::TxnResponse", nullptr, 0, eNodeType::EtcdNodeService};
    gMessageInfo[KVCompactMessageId] = RpcService{"KV", "Compact", "::etcdserverpb::CompactionRequest", "::etcdserverpb::CompactionResponse", nullptr, 0, eNodeType::EtcdNodeService};
    gMessageInfo[LeaseLeaseGrantMessageId] = RpcService{"Lease", "LeaseGrant", "::etcdserverpb::LeaseGrantRequest", "::etcdserverpb::LeaseGrantResponse", nullptr, 0, eNodeType::EtcdNodeService};
    gMessageInfo[LeaseLeaseRevokeMessageId] = RpcService{"Lease", "LeaseRevoke", "::etcdserverpb::LeaseRevokeRequest", "::etcdserverpb::LeaseRevokeResponse", nullptr, 0, eNodeType::EtcdNodeService};
    gMessageInfo[LeaseLeaseKeepAliveMessageId] = RpcService{"Lease", "LeaseKeepAlive", "::etcdserverpb::LeaseKeepAliveRequest", "::etcdserverpb::LeaseKeepAliveResponse", nullptr, 0, eNodeType::EtcdNodeService};
    gMessageInfo[LeaseLeaseTimeToLiveMessageId] = RpcService{"Lease", "LeaseTimeToLive", "::etcdserverpb::LeaseTimeToLiveRequest", "::etcdserverpb::LeaseTimeToLiveResponse", nullptr, 0, eNodeType::EtcdNodeService};
    gMessageInfo[LeaseLeaseLeasesMessageId] = RpcService{"Lease", "LeaseLeases", "::etcdserverpb::LeaseLeasesRequest", "::etcdserverpb::LeaseLeasesResponse", nullptr, 0, eNodeType::EtcdNodeService};
    gMessageInfo[LoginServiceLoginMessageId] = RpcService{"LoginService", "Login", "::loginpb::LoginC2LRequest", "::loginpb::LoginC2LResponse", nullptr, 1, eNodeType::LoginNodeService};
    gMessageInfo[LoginServiceCreatePlayerMessageId] = RpcService{"LoginService", "CreatePlayer", "::loginpb::CreatePlayerC2LRequest", "::loginpb::CreatePlayerC2LResponse", nullptr, 1, eNodeType::LoginNodeService};
    gMessageInfo[LoginServiceEnterGameMessageId] = RpcService{"LoginService", "EnterGame", "::loginpb::EnterGameC2LRequest", "::loginpb::EnterGameC2LResponse", nullptr, 1, eNodeType::LoginNodeService};
    gMessageInfo[LoginServiceLeaveGameMessageId] = RpcService{"LoginService", "LeaveGame", "::loginpb::LeaveGameC2LRequest", "::Empty", nullptr, 1, eNodeType::LoginNodeService};
    gMessageInfo[LoginServiceDisconnectMessageId] = RpcService{"LoginService", "Disconnect", "::loginpb::LoginNodeDisconnectRequest", "::Empty", nullptr, 1, eNodeType::LoginNodeService};
    gMessageInfo[PlayerDBServiceLoad2RedisMessageId] = RpcService{"PlayerDBService", "Load2Redis", "::LoadPlayerRequest", "::LoadPlayerResponse", nullptr, 0, eNodeType::DbNodeService};
    gMessageInfo[PlayerDBServiceSave2RedisMessageId] = RpcService{"PlayerDBService", "Save2Redis", "::SavePlayerRequest", "::SavePlayerResponse", nullptr, 0, eNodeType::DbNodeService};
    gMessageInfo[WatchWatchMessageId] = RpcService{"Watch", "Watch", "::etcdserverpb::WatchRequest", "::etcdserverpb::WatchResponse", nullptr, 0, eNodeType::EtcdNodeService};
    gClientToServerMessageId.emplace(CentreClientPlayerCommonServiceSendTipToClientMessageId);
    gClientToServerMessageId.emplace(CentreClientPlayerCommonServiceKickPlayerMessageId);
    gClientToServerMessageId.emplace(ClientPlayerSceneServiceEnterSceneMessageId);
    gClientToServerMessageId.emplace(ClientPlayerSceneServiceNotifyEnterSceneMessageId);
    gClientToServerMessageId.emplace(ClientPlayerSceneServiceSceneInfoC2SMessageId);
    gClientToServerMessageId.emplace(ClientPlayerSceneServiceNotifySceneInfoMessageId);
    gClientToServerMessageId.emplace(ClientPlayerSceneServiceNotifyActorCreateMessageId);
    gClientToServerMessageId.emplace(ClientPlayerSceneServiceNotifyActorDestroyMessageId);
    gClientToServerMessageId.emplace(ClientPlayerSceneServiceNotifyActorListCreateMessageId);
    gClientToServerMessageId.emplace(ClientPlayerSceneServiceNotifyActorListDestroyMessageId);
    gClientToServerMessageId.emplace(ClientPlayerSkillServiceReleaseSkillMessageId);
    gClientToServerMessageId.emplace(ClientPlayerSkillServiceNotifySkillUsedMessageId);
    gClientToServerMessageId.emplace(ClientPlayerSkillServiceNotifySkillInterruptedMessageId);
    gClientToServerMessageId.emplace(ClientPlayerSkillServiceGetSkillListMessageId);
    gClientToServerMessageId.emplace(GameClientPlayerCommonServiceSendTipToClientMessageId);
    gClientToServerMessageId.emplace(GameClientPlayerCommonServiceKickPlayerMessageId);
}
