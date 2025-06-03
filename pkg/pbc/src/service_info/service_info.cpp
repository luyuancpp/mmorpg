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

std::unordered_set<uint32_t> gAllowedClientMessageIds;
std::array<RpcService, 85> gRpcServiceByMessageId;

void InitMessageInfo()
{
    gRpcServiceByMessageId[AccountDBServiceLoad2RedisMessageId] = RpcService{"AccountDBService", "Load2Redis", "::LoadAccountRequest", "::LoadAccountResponse", nullptr, 0, eNodeType::DbNodeService};
    gRpcServiceByMessageId[AccountDBServiceSave2RedisMessageId] = RpcService{"AccountDBService", "Save2Redis", "::SaveAccountRequest", "::SaveAccountResponse", nullptr, 0, eNodeType::DbNodeService};
    gRpcServiceByMessageId[CentreClientPlayerCommonServiceSendTipToClientMessageId] = RpcService{"CentreClientPlayerCommonService", "SendTipToClient", "::TipInfoMessage", "::Empty", std::make_unique_for_overwrite<CentreClientPlayerCommonServiceImpl>(), 0, eNodeType::CentreNodeService};
    gRpcServiceByMessageId[CentreClientPlayerCommonServiceKickPlayerMessageId] = RpcService{"CentreClientPlayerCommonService", "KickPlayer", "::CentreKickPlayerRequest", "::Empty", std::make_unique_for_overwrite<CentreClientPlayerCommonServiceImpl>(), 0, eNodeType::CentreNodeService};
    gRpcServiceByMessageId[CentrePlayerSceneServiceEnterSceneMessageId] = RpcService{"CentrePlayerSceneService", "EnterScene", "::CentreEnterSceneRequest", "::google::protobuf::Empty", std::make_unique_for_overwrite<CentrePlayerSceneServiceImpl>(), 0, eNodeType::CentreNodeService};
    gRpcServiceByMessageId[CentrePlayerSceneServiceLeaveSceneMessageId] = RpcService{"CentrePlayerSceneService", "LeaveScene", "::CentreLeaveSceneRequest", "::google::protobuf::Empty", std::make_unique_for_overwrite<CentrePlayerSceneServiceImpl>(), 0, eNodeType::CentreNodeService};
    gRpcServiceByMessageId[CentrePlayerSceneServiceLeaveSceneAsyncSavePlayerCompleteMessageId] = RpcService{"CentrePlayerSceneService", "LeaveSceneAsyncSavePlayerComplete", "::CentreLeaveSceneAsyncSavePlayerCompleteRequest", "::google::protobuf::Empty", std::make_unique_for_overwrite<CentrePlayerSceneServiceImpl>(), 0, eNodeType::CentreNodeService};
    gRpcServiceByMessageId[CentrePlayerSceneServiceSceneInfoC2SMessageId] = RpcService{"CentrePlayerSceneService", "SceneInfoC2S", "::CentreSceneInfoRequest", "::google::protobuf::Empty", std::make_unique_for_overwrite<CentrePlayerSceneServiceImpl>(), 0, eNodeType::CentreNodeService};
    gRpcServiceByMessageId[CentrePlayerServiceTestMessageId] = RpcService{"CentrePlayerService", "Test", "::google::protobuf::Empty", "::google::protobuf::Empty", std::make_unique_for_overwrite<CentrePlayerServiceImpl>(), 0, eNodeType::CentreNodeService};
    gRpcServiceByMessageId[CentreSceneServiceRegisterSceneMessageId] = RpcService{"CentreSceneService", "RegisterScene", "::RegisterSceneRequest", "::RegisterSceneResponse", std::make_unique_for_overwrite<CentreSceneServiceImpl>(), 0, eNodeType::CentreNodeService};
    gRpcServiceByMessageId[CentreSceneServiceUnRegisterSceneMessageId] = RpcService{"CentreSceneService", "UnRegisterScene", "::UnRegisterSceneRequest", "::Empty", std::make_unique_for_overwrite<CentreSceneServiceImpl>(), 0, eNodeType::CentreNodeService};
    gRpcServiceByMessageId[CentreServiceGatePlayerServiceMessageId] = RpcService{"CentreService", "GatePlayerService", "::GateClientMessageRequest", "::Empty", std::make_unique_for_overwrite<CentreServiceImpl>(), 0, eNodeType::CentreNodeService};
    gRpcServiceByMessageId[CentreServiceGateSessionDisconnectMessageId] = RpcService{"CentreService", "GateSessionDisconnect", "::GateSessionDisconnectRequest", "::Empty", std::make_unique_for_overwrite<CentreServiceImpl>(), 0, eNodeType::CentreNodeService};
    gRpcServiceByMessageId[CentreServiceLoginNodeAccountLoginMessageId] = RpcService{"CentreService", "LoginNodeAccountLogin", "::LoginRequest", "::LoginResponse", std::make_unique_for_overwrite<CentreServiceImpl>(), 0, eNodeType::CentreNodeService};
    gRpcServiceByMessageId[CentreServiceLoginNodeEnterGameMessageId] = RpcService{"CentreService", "LoginNodeEnterGame", "::CentrePlayerGameNodeEntryRequest", "::Empty", std::make_unique_for_overwrite<CentreServiceImpl>(), 0, eNodeType::CentreNodeService};
    gRpcServiceByMessageId[CentreServiceLoginNodeLeaveGameMessageId] = RpcService{"CentreService", "LoginNodeLeaveGame", "::LoginNodeLeaveGameRequest", "::Empty", std::make_unique_for_overwrite<CentreServiceImpl>(), 0, eNodeType::CentreNodeService};
    gRpcServiceByMessageId[CentreServiceLoginNodeSessionDisconnectMessageId] = RpcService{"CentreService", "LoginNodeSessionDisconnect", "::GateSessionDisconnectRequest", "::Empty", std::make_unique_for_overwrite<CentreServiceImpl>(), 0, eNodeType::CentreNodeService};
    gRpcServiceByMessageId[CentreServicePlayerServiceMessageId] = RpcService{"CentreService", "PlayerService", "::NodeRouteMessageRequest", "::NodeRouteMessageResponse", std::make_unique_for_overwrite<CentreServiceImpl>(), 0, eNodeType::CentreNodeService};
    gRpcServiceByMessageId[CentreServiceEnterGsSucceedMessageId] = RpcService{"CentreService", "EnterGsSucceed", "::EnterGameNodeSuccessRequest", "::Empty", std::make_unique_for_overwrite<CentreServiceImpl>(), 0, eNodeType::CentreNodeService};
    gRpcServiceByMessageId[CentreServiceRouteNodeStringMsgMessageId] = RpcService{"CentreService", "RouteNodeStringMsg", "::RouteMessageRequest", "::RouteMessageResponse", std::make_unique_for_overwrite<CentreServiceImpl>(), 0, eNodeType::CentreNodeService};
    gRpcServiceByMessageId[CentreServiceRoutePlayerStringMsgMessageId] = RpcService{"CentreService", "RoutePlayerStringMsg", "::RoutePlayerMessageRequest", "::RoutePlayerMessageResponse", std::make_unique_for_overwrite<CentreServiceImpl>(), 0, eNodeType::CentreNodeService};
    gRpcServiceByMessageId[CentreServiceInitSceneNodeMessageId] = RpcService{"CentreService", "InitSceneNode", "::InitSceneNodeRequest", "::Empty", std::make_unique_for_overwrite<CentreServiceImpl>(), 0, eNodeType::CentreNodeService};
    gRpcServiceByMessageId[CentreServiceRegisterNodeSessionMessageId] = RpcService{"CentreService", "RegisterNodeSession", "::RegisterNodeSessionRequest", "::RegisterNodeSessionResponse", std::make_unique_for_overwrite<CentreServiceImpl>(), 0, eNodeType::CentreNodeService};
    gRpcServiceByMessageId[ClientPlayerSceneServiceEnterSceneMessageId] = RpcService{"ClientPlayerSceneService", "EnterScene", "::EnterSceneC2SRequest", "::EnterSceneC2SResponse", std::make_unique_for_overwrite<ClientPlayerSceneServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceByMessageId[ClientPlayerSceneServiceNotifyEnterSceneMessageId] = RpcService{"ClientPlayerSceneService", "NotifyEnterScene", "::EnterSceneS2C", "::Empty", std::make_unique_for_overwrite<ClientPlayerSceneServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceByMessageId[ClientPlayerSceneServiceSceneInfoC2SMessageId] = RpcService{"ClientPlayerSceneService", "SceneInfoC2S", "::SceneInfoRequest", "::Empty", std::make_unique_for_overwrite<ClientPlayerSceneServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceByMessageId[ClientPlayerSceneServiceNotifySceneInfoMessageId] = RpcService{"ClientPlayerSceneService", "NotifySceneInfo", "::SceneInfoS2C", "::Empty", std::make_unique_for_overwrite<ClientPlayerSceneServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceByMessageId[ClientPlayerSceneServiceNotifyActorCreateMessageId] = RpcService{"ClientPlayerSceneService", "NotifyActorCreate", "::ActorCreateS2C", "::Empty", std::make_unique_for_overwrite<ClientPlayerSceneServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceByMessageId[ClientPlayerSceneServiceNotifyActorDestroyMessageId] = RpcService{"ClientPlayerSceneService", "NotifyActorDestroy", "::ActorDestroyS2C", "::Empty", std::make_unique_for_overwrite<ClientPlayerSceneServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceByMessageId[ClientPlayerSceneServiceNotifyActorListCreateMessageId] = RpcService{"ClientPlayerSceneService", "NotifyActorListCreate", "::ActorListCreateS2C", "::Empty", std::make_unique_for_overwrite<ClientPlayerSceneServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceByMessageId[ClientPlayerSceneServiceNotifyActorListDestroyMessageId] = RpcService{"ClientPlayerSceneService", "NotifyActorListDestroy", "::ActorListDestroyS2C", "::Empty", std::make_unique_for_overwrite<ClientPlayerSceneServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceByMessageId[ClientPlayerSkillServiceReleaseSkillMessageId] = RpcService{"ClientPlayerSkillService", "ReleaseSkill", "::ReleaseSkillSkillRequest", "::ReleaseSkillSkillResponse", std::make_unique_for_overwrite<ClientPlayerSkillServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceByMessageId[ClientPlayerSkillServiceNotifySkillUsedMessageId] = RpcService{"ClientPlayerSkillService", "NotifySkillUsed", "::SkillUsedS2C", "::Empty", std::make_unique_for_overwrite<ClientPlayerSkillServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceByMessageId[ClientPlayerSkillServiceNotifySkillInterruptedMessageId] = RpcService{"ClientPlayerSkillService", "NotifySkillInterrupted", "::SkillInterruptedS2C", "::Empty", std::make_unique_for_overwrite<ClientPlayerSkillServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceByMessageId[ClientPlayerSkillServiceGetSkillListMessageId] = RpcService{"ClientPlayerSkillService", "GetSkillList", "::GetSkillListRequest", "::GetSkillListResponse", std::make_unique_for_overwrite<ClientPlayerSkillServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceByMessageId[EntitySyncServiceSyncBaseAttributeMessageId] = RpcService{"EntitySyncService", "SyncBaseAttribute", "::BaseAttributeSyncDataS2C", "::Empty", std::make_unique_for_overwrite<EntitySyncServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceByMessageId[EntitySyncServiceSyncAttribute2FramesMessageId] = RpcService{"EntitySyncService", "SyncAttribute2Frames", "::AttributeDelta2FramesS2C", "::Empty", std::make_unique_for_overwrite<EntitySyncServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceByMessageId[EntitySyncServiceSyncAttribute5FramesMessageId] = RpcService{"EntitySyncService", "SyncAttribute5Frames", "::AttributeDelta5FramesS2C", "::Empty", std::make_unique_for_overwrite<EntitySyncServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceByMessageId[EntitySyncServiceSyncAttribute10FramesMessageId] = RpcService{"EntitySyncService", "SyncAttribute10Frames", "::AttributeDelta10FramesS2C", "::Empty", std::make_unique_for_overwrite<EntitySyncServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceByMessageId[EntitySyncServiceSyncAttribute30FramesMessageId] = RpcService{"EntitySyncService", "SyncAttribute30Frames", "::AttributeDelta30FramesS2C", "::Empty", std::make_unique_for_overwrite<EntitySyncServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceByMessageId[EntitySyncServiceSyncAttribute60FramesMessageId] = RpcService{"EntitySyncService", "SyncAttribute60Frames", "::AttributeDelta60FramesS2C", "::Empty", std::make_unique_for_overwrite<EntitySyncServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceByMessageId[GameClientPlayerCommonServiceSendTipToClientMessageId] = RpcService{"GameClientPlayerCommonService", "SendTipToClient", "::TipInfoMessage", "::Empty", std::make_unique_for_overwrite<GameClientPlayerCommonServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceByMessageId[GameClientPlayerCommonServiceKickPlayerMessageId] = RpcService{"GameClientPlayerCommonService", "KickPlayer", "::GameKickPlayerRequest", "::Empty", std::make_unique_for_overwrite<GameClientPlayerCommonServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceByMessageId[GamePlayerSceneServiceEnterSceneMessageId] = RpcService{"GamePlayerSceneService", "EnterScene", "::GsEnterSceneRequest", "::google::protobuf::Empty", std::make_unique_for_overwrite<GamePlayerSceneServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceByMessageId[GamePlayerSceneServiceLeaveSceneMessageId] = RpcService{"GamePlayerSceneService", "LeaveScene", "::GsLeaveSceneRequest", "::google::protobuf::Empty", std::make_unique_for_overwrite<GamePlayerSceneServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceByMessageId[GamePlayerSceneServiceEnterSceneS2CMessageId] = RpcService{"GamePlayerSceneService", "EnterSceneS2C", "::EnterSceneS2CRequest", "::EnterScenerS2CResponse", std::make_unique_for_overwrite<GamePlayerSceneServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceByMessageId[GamePlayerServiceCentre2GsLoginMessageId] = RpcService{"GamePlayerService", "Centre2GsLogin", "::Centre2GsLoginRequest", "::google::protobuf::Empty", std::make_unique_for_overwrite<GamePlayerServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceByMessageId[GamePlayerServiceExitGameMessageId] = RpcService{"GamePlayerService", "ExitGame", "::GameNodeExitGameRequest", "::google::protobuf::Empty", std::make_unique_for_overwrite<GamePlayerServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceByMessageId[GameSceneServiceTestMessageId] = RpcService{"GameSceneService", "Test", "::GameSceneTest", "::Empty", std::make_unique_for_overwrite<GameSceneServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceByMessageId[GameServicePlayerEnterGameNodeMessageId] = RpcService{"GameService", "PlayerEnterGameNode", "::PlayerEnterGameNodeRequest", "::Empty", std::make_unique_for_overwrite<GameServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceByMessageId[GameServiceSendMessageToPlayerMessageId] = RpcService{"GameService", "SendMessageToPlayer", "::NodeRouteMessageRequest", "::NodeRouteMessageResponse", std::make_unique_for_overwrite<GameServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceByMessageId[GameServiceClientSendMessageToPlayerMessageId] = RpcService{"GameService", "ClientSendMessageToPlayer", "::ClientSendMessageToPlayerRequest", "::ClientSendMessageToPlayerResponse", std::make_unique_for_overwrite<GameServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceByMessageId[GameServiceCentreSendToPlayerViaGameNodeMessageId] = RpcService{"GameService", "CentreSendToPlayerViaGameNode", "::NodeRouteMessageRequest", "::Empty", std::make_unique_for_overwrite<GameServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceByMessageId[GameServiceInvokePlayerServiceMessageId] = RpcService{"GameService", "InvokePlayerService", "::NodeRouteMessageRequest", "::NodeRouteMessageResponse", std::make_unique_for_overwrite<GameServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceByMessageId[GameServiceRouteNodeStringMsgMessageId] = RpcService{"GameService", "RouteNodeStringMsg", "::RouteMessageRequest", "::RouteMessageResponse", std::make_unique_for_overwrite<GameServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceByMessageId[GameServiceRoutePlayerStringMsgMessageId] = RpcService{"GameService", "RoutePlayerStringMsg", "::RoutePlayerMessageRequest", "::RoutePlayerMessageResponse", std::make_unique_for_overwrite<GameServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceByMessageId[GameServiceUpdateSessionDetailMessageId] = RpcService{"GameService", "UpdateSessionDetail", "::RegisterPlayerSessionRequest", "::Empty", std::make_unique_for_overwrite<GameServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceByMessageId[GameServiceEnterSceneMessageId] = RpcService{"GameService", "EnterScene", "::Centre2GsEnterSceneRequest", "::Empty", std::make_unique_for_overwrite<GameServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceByMessageId[GameServiceCreateSceneMessageId] = RpcService{"GameService", "CreateScene", "::CreateSceneRequest", "::CreateSceneResponse", std::make_unique_for_overwrite<GameServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceByMessageId[GameServiceRegisterNodeSessionMessageId] = RpcService{"GameService", "RegisterNodeSession", "::RegisterNodeSessionRequest", "::RegisterNodeSessionResponse", std::make_unique_for_overwrite<GameServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceByMessageId[GateServicePlayerEnterGameNodeMessageId] = RpcService{"GateService", "PlayerEnterGameNode", "::RegisterGameNodeSessionRequest", "::RegisterGameNodeSessionResponse", std::make_unique_for_overwrite<GateServiceImpl>(), 0, eNodeType::GateNodeService};
    gRpcServiceByMessageId[GateServiceSendMessageToPlayerMessageId] = RpcService{"GateService", "SendMessageToPlayer", "::NodeRouteMessageRequest", "::Empty", std::make_unique_for_overwrite<GateServiceImpl>(), 0, eNodeType::GateNodeService};
    gRpcServiceByMessageId[GateServiceKickSessionByCentreMessageId] = RpcService{"GateService", "KickSessionByCentre", "::KickSessionRequest", "::Empty", std::make_unique_for_overwrite<GateServiceImpl>(), 0, eNodeType::GateNodeService};
    gRpcServiceByMessageId[GateServiceRouteNodeMessageMessageId] = RpcService{"GateService", "RouteNodeMessage", "::RouteMessageRequest", "::RouteMessageResponse", std::make_unique_for_overwrite<GateServiceImpl>(), 0, eNodeType::GateNodeService};
    gRpcServiceByMessageId[GateServiceRoutePlayerMessageMessageId] = RpcService{"GateService", "RoutePlayerMessage", "::RoutePlayerMessageRequest", "::RoutePlayerMessageResponse", std::make_unique_for_overwrite<GateServiceImpl>(), 0, eNodeType::GateNodeService};
    gRpcServiceByMessageId[GateServiceBroadcastToPlayersMessageId] = RpcService{"GateService", "BroadcastToPlayers", "::BroadcastToPlayersRequest", "::Empty", std::make_unique_for_overwrite<GateServiceImpl>(), 0, eNodeType::GateNodeService};
    gRpcServiceByMessageId[GateServiceRegisterNodeSessionMessageId] = RpcService{"GateService", "RegisterNodeSession", "::RegisterNodeSessionRequest", "::RegisterNodeSessionResponse", std::make_unique_for_overwrite<GateServiceImpl>(), 0, eNodeType::GateNodeService};
    gRpcServiceByMessageId[KVRangeMessageId] = RpcService{"KV", "Range", "::etcdserverpb::RangeRequest", "::etcdserverpb::RangeResponse", nullptr, 0, eNodeType::EtcdNodeService};
    gRpcServiceByMessageId[KVPutMessageId] = RpcService{"KV", "Put", "::etcdserverpb::PutRequest", "::etcdserverpb::PutResponse", nullptr, 0, eNodeType::EtcdNodeService};
    gRpcServiceByMessageId[KVDeleteRangeMessageId] = RpcService{"KV", "DeleteRange", "::etcdserverpb::DeleteRangeRequest", "::etcdserverpb::DeleteRangeResponse", nullptr, 0, eNodeType::EtcdNodeService};
    gRpcServiceByMessageId[KVTxnMessageId] = RpcService{"KV", "Txn", "::etcdserverpb::TxnRequest", "::etcdserverpb::TxnResponse", nullptr, 0, eNodeType::EtcdNodeService};
    gRpcServiceByMessageId[KVCompactMessageId] = RpcService{"KV", "Compact", "::etcdserverpb::CompactionRequest", "::etcdserverpb::CompactionResponse", nullptr, 0, eNodeType::EtcdNodeService};
    gRpcServiceByMessageId[LeaseLeaseGrantMessageId] = RpcService{"Lease", "LeaseGrant", "::etcdserverpb::LeaseGrantRequest", "::etcdserverpb::LeaseGrantResponse", nullptr, 0, eNodeType::EtcdNodeService};
    gRpcServiceByMessageId[LeaseLeaseRevokeMessageId] = RpcService{"Lease", "LeaseRevoke", "::etcdserverpb::LeaseRevokeRequest", "::etcdserverpb::LeaseRevokeResponse", nullptr, 0, eNodeType::EtcdNodeService};
    gRpcServiceByMessageId[LeaseLeaseKeepAliveMessageId] = RpcService{"Lease", "LeaseKeepAlive", "::etcdserverpb::LeaseKeepAliveRequest", "::etcdserverpb::LeaseKeepAliveResponse", nullptr, 0, eNodeType::EtcdNodeService};
    gRpcServiceByMessageId[LeaseLeaseTimeToLiveMessageId] = RpcService{"Lease", "LeaseTimeToLive", "::etcdserverpb::LeaseTimeToLiveRequest", "::etcdserverpb::LeaseTimeToLiveResponse", nullptr, 0, eNodeType::EtcdNodeService};
    gRpcServiceByMessageId[LeaseLeaseLeasesMessageId] = RpcService{"Lease", "LeaseLeases", "::etcdserverpb::LeaseLeasesRequest", "::etcdserverpb::LeaseLeasesResponse", nullptr, 0, eNodeType::EtcdNodeService};
    gRpcServiceByMessageId[LoginServiceLoginMessageId] = RpcService{"LoginService", "Login", "::loginpb::LoginC2LRequest", "::loginpb::LoginC2LResponse", nullptr, 1, eNodeType::LoginNodeService};
    gRpcServiceByMessageId[LoginServiceCreatePlayerMessageId] = RpcService{"LoginService", "CreatePlayer", "::loginpb::CreatePlayerC2LRequest", "::loginpb::CreatePlayerC2LResponse", nullptr, 1, eNodeType::LoginNodeService};
    gRpcServiceByMessageId[LoginServiceEnterGameMessageId] = RpcService{"LoginService", "EnterGame", "::loginpb::EnterGameC2LRequest", "::loginpb::EnterGameC2LResponse", nullptr, 1, eNodeType::LoginNodeService};
    gRpcServiceByMessageId[LoginServiceLeaveGameMessageId] = RpcService{"LoginService", "LeaveGame", "::loginpb::LeaveGameC2LRequest", "::Empty", nullptr, 1, eNodeType::LoginNodeService};
    gRpcServiceByMessageId[LoginServiceDisconnectMessageId] = RpcService{"LoginService", "Disconnect", "::loginpb::LoginNodeDisconnectRequest", "::Empty", nullptr, 1, eNodeType::LoginNodeService};
    gRpcServiceByMessageId[PlayerDBServiceLoad2RedisMessageId] = RpcService{"PlayerDBService", "Load2Redis", "::LoadPlayerRequest", "::LoadPlayerResponse", nullptr, 0, eNodeType::DbNodeService};
    gRpcServiceByMessageId[PlayerDBServiceSave2RedisMessageId] = RpcService{"PlayerDBService", "Save2Redis", "::SavePlayerRequest", "::SavePlayerResponse", nullptr, 0, eNodeType::DbNodeService};
    gRpcServiceByMessageId[WatchWatchMessageId] = RpcService{"Watch", "Watch", "::etcdserverpb::WatchRequest", "::etcdserverpb::WatchResponse", nullptr, 0, eNodeType::EtcdNodeService};
    gAllowedClientMessageIds.emplace(CentreClientPlayerCommonServiceSendTipToClientMessageId);
    gAllowedClientMessageIds.emplace(CentreClientPlayerCommonServiceKickPlayerMessageId);
    gAllowedClientMessageIds.emplace(ClientPlayerSceneServiceEnterSceneMessageId);
    gAllowedClientMessageIds.emplace(ClientPlayerSceneServiceNotifyEnterSceneMessageId);
    gAllowedClientMessageIds.emplace(ClientPlayerSceneServiceSceneInfoC2SMessageId);
    gAllowedClientMessageIds.emplace(ClientPlayerSceneServiceNotifySceneInfoMessageId);
    gAllowedClientMessageIds.emplace(ClientPlayerSceneServiceNotifyActorCreateMessageId);
    gAllowedClientMessageIds.emplace(ClientPlayerSceneServiceNotifyActorDestroyMessageId);
    gAllowedClientMessageIds.emplace(ClientPlayerSceneServiceNotifyActorListCreateMessageId);
    gAllowedClientMessageIds.emplace(ClientPlayerSceneServiceNotifyActorListDestroyMessageId);
    gAllowedClientMessageIds.emplace(ClientPlayerSkillServiceReleaseSkillMessageId);
    gAllowedClientMessageIds.emplace(ClientPlayerSkillServiceNotifySkillUsedMessageId);
    gAllowedClientMessageIds.emplace(ClientPlayerSkillServiceNotifySkillInterruptedMessageId);
    gAllowedClientMessageIds.emplace(ClientPlayerSkillServiceGetSkillListMessageId);
    gAllowedClientMessageIds.emplace(GameClientPlayerCommonServiceSendTipToClientMessageId);
    gAllowedClientMessageIds.emplace(GameClientPlayerCommonServiceKickPlayerMessageId);
}
