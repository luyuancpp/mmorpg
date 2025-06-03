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
    gRpcServiceByMessageId[AccountDBServiceLoad2RedisMessageId] = RpcService{"AccountDBService", "Load2Redis", std::make_unique_for_overwrite<::LoadAccountRequest>(), std::make_unique_for_overwrite<::LoadAccountResponse>(), nullptr, 0, eNodeType::DbNodeService};
    gRpcServiceByMessageId[AccountDBServiceSave2RedisMessageId] = RpcService{"AccountDBService", "Save2Redis", std::make_unique_for_overwrite<::SaveAccountRequest>(), std::make_unique_for_overwrite<::SaveAccountResponse>(), nullptr, 0, eNodeType::DbNodeService};
    gRpcServiceByMessageId[CentreClientPlayerCommonServiceSendTipToClientMessageId] = RpcService{"CentreClientPlayerCommonService", "SendTipToClient", std::make_unique_for_overwrite<::TipInfoMessage>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<CentreClientPlayerCommonServiceImpl>(), 0, eNodeType::CentreNodeService};
    gRpcServiceByMessageId[CentreClientPlayerCommonServiceKickPlayerMessageId] = RpcService{"CentreClientPlayerCommonService", "KickPlayer", std::make_unique_for_overwrite<::CentreKickPlayerRequest>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<CentreClientPlayerCommonServiceImpl>(), 0, eNodeType::CentreNodeService};
    gRpcServiceByMessageId[CentrePlayerSceneServiceEnterSceneMessageId] = RpcService{"CentrePlayerSceneService", "EnterScene", std::make_unique_for_overwrite<::CentreEnterSceneRequest>(), std::make_unique_for_overwrite<::google::protobuf::Empty>(), std::make_unique_for_overwrite<CentrePlayerSceneServiceImpl>(), 0, eNodeType::CentreNodeService};
    gRpcServiceByMessageId[CentrePlayerSceneServiceLeaveSceneMessageId] = RpcService{"CentrePlayerSceneService", "LeaveScene", std::make_unique_for_overwrite<::CentreLeaveSceneRequest>(), std::make_unique_for_overwrite<::google::protobuf::Empty>(), std::make_unique_for_overwrite<CentrePlayerSceneServiceImpl>(), 0, eNodeType::CentreNodeService};
    gRpcServiceByMessageId[CentrePlayerSceneServiceLeaveSceneAsyncSavePlayerCompleteMessageId] = RpcService{"CentrePlayerSceneService", "LeaveSceneAsyncSavePlayerComplete", std::make_unique_for_overwrite<::CentreLeaveSceneAsyncSavePlayerCompleteRequest>(), std::make_unique_for_overwrite<::google::protobuf::Empty>(), std::make_unique_for_overwrite<CentrePlayerSceneServiceImpl>(), 0, eNodeType::CentreNodeService};
    gRpcServiceByMessageId[CentrePlayerSceneServiceSceneInfoC2SMessageId] = RpcService{"CentrePlayerSceneService", "SceneInfoC2S", std::make_unique_for_overwrite<::CentreSceneInfoRequest>(), std::make_unique_for_overwrite<::google::protobuf::Empty>(), std::make_unique_for_overwrite<CentrePlayerSceneServiceImpl>(), 0, eNodeType::CentreNodeService};
    gRpcServiceByMessageId[CentrePlayerServiceTestMessageId] = RpcService{"CentrePlayerService", "Test", std::make_unique_for_overwrite<::google::protobuf::Empty>(), std::make_unique_for_overwrite<::google::protobuf::Empty>(), std::make_unique_for_overwrite<CentrePlayerServiceImpl>(), 0, eNodeType::CentreNodeService};
    gRpcServiceByMessageId[CentreSceneServiceRegisterSceneMessageId] = RpcService{"CentreSceneService", "RegisterScene", std::make_unique_for_overwrite<::RegisterSceneRequest>(), std::make_unique_for_overwrite<::RegisterSceneResponse>(), std::make_unique_for_overwrite<CentreSceneServiceImpl>(), 0, eNodeType::CentreNodeService};
    gRpcServiceByMessageId[CentreSceneServiceUnRegisterSceneMessageId] = RpcService{"CentreSceneService", "UnRegisterScene", std::make_unique_for_overwrite<::UnRegisterSceneRequest>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<CentreSceneServiceImpl>(), 0, eNodeType::CentreNodeService};
    gRpcServiceByMessageId[CentreServiceGatePlayerServiceMessageId] = RpcService{"CentreService", "GatePlayerService", std::make_unique_for_overwrite<::GateClientMessageRequest>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<CentreServiceImpl>(), 0, eNodeType::CentreNodeService};
    gRpcServiceByMessageId[CentreServiceGateSessionDisconnectMessageId] = RpcService{"CentreService", "GateSessionDisconnect", std::make_unique_for_overwrite<::GateSessionDisconnectRequest>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<CentreServiceImpl>(), 0, eNodeType::CentreNodeService};
    gRpcServiceByMessageId[CentreServiceLoginNodeAccountLoginMessageId] = RpcService{"CentreService", "LoginNodeAccountLogin", std::make_unique_for_overwrite<::LoginRequest>(), std::make_unique_for_overwrite<::LoginResponse>(), std::make_unique_for_overwrite<CentreServiceImpl>(), 0, eNodeType::CentreNodeService};
    gRpcServiceByMessageId[CentreServiceLoginNodeEnterGameMessageId] = RpcService{"CentreService", "LoginNodeEnterGame", std::make_unique_for_overwrite<::CentrePlayerGameNodeEntryRequest>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<CentreServiceImpl>(), 0, eNodeType::CentreNodeService};
    gRpcServiceByMessageId[CentreServiceLoginNodeLeaveGameMessageId] = RpcService{"CentreService", "LoginNodeLeaveGame", std::make_unique_for_overwrite<::LoginNodeLeaveGameRequest>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<CentreServiceImpl>(), 0, eNodeType::CentreNodeService};
    gRpcServiceByMessageId[CentreServiceLoginNodeSessionDisconnectMessageId] = RpcService{"CentreService", "LoginNodeSessionDisconnect", std::make_unique_for_overwrite<::GateSessionDisconnectRequest>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<CentreServiceImpl>(), 0, eNodeType::CentreNodeService};
    gRpcServiceByMessageId[CentreServicePlayerServiceMessageId] = RpcService{"CentreService", "PlayerService", std::make_unique_for_overwrite<::NodeRouteMessageRequest>(), std::make_unique_for_overwrite<::NodeRouteMessageResponse>(), std::make_unique_for_overwrite<CentreServiceImpl>(), 0, eNodeType::CentreNodeService};
    gRpcServiceByMessageId[CentreServiceEnterGsSucceedMessageId] = RpcService{"CentreService", "EnterGsSucceed", std::make_unique_for_overwrite<::EnterGameNodeSuccessRequest>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<CentreServiceImpl>(), 0, eNodeType::CentreNodeService};
    gRpcServiceByMessageId[CentreServiceRouteNodeStringMsgMessageId] = RpcService{"CentreService", "RouteNodeStringMsg", std::make_unique_for_overwrite<::RouteMessageRequest>(), std::make_unique_for_overwrite<::RouteMessageResponse>(), std::make_unique_for_overwrite<CentreServiceImpl>(), 0, eNodeType::CentreNodeService};
    gRpcServiceByMessageId[CentreServiceRoutePlayerStringMsgMessageId] = RpcService{"CentreService", "RoutePlayerStringMsg", std::make_unique_for_overwrite<::RoutePlayerMessageRequest>(), std::make_unique_for_overwrite<::RoutePlayerMessageResponse>(), std::make_unique_for_overwrite<CentreServiceImpl>(), 0, eNodeType::CentreNodeService};
    gRpcServiceByMessageId[CentreServiceInitSceneNodeMessageId] = RpcService{"CentreService", "InitSceneNode", std::make_unique_for_overwrite<::InitSceneNodeRequest>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<CentreServiceImpl>(), 0, eNodeType::CentreNodeService};
    gRpcServiceByMessageId[CentreServiceRegisterNodeSessionMessageId] = RpcService{"CentreService", "RegisterNodeSession", std::make_unique_for_overwrite<::RegisterNodeSessionRequest>(), std::make_unique_for_overwrite<::RegisterNodeSessionResponse>(), std::make_unique_for_overwrite<CentreServiceImpl>(), 0, eNodeType::CentreNodeService};
    gRpcServiceByMessageId[ClientPlayerSceneServiceEnterSceneMessageId] = RpcService{"ClientPlayerSceneService", "EnterScene", std::make_unique_for_overwrite<::EnterSceneC2SRequest>(), std::make_unique_for_overwrite<::EnterSceneC2SResponse>(), std::make_unique_for_overwrite<ClientPlayerSceneServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceByMessageId[ClientPlayerSceneServiceNotifyEnterSceneMessageId] = RpcService{"ClientPlayerSceneService", "NotifyEnterScene", std::make_unique_for_overwrite<::EnterSceneS2C>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<ClientPlayerSceneServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceByMessageId[ClientPlayerSceneServiceSceneInfoC2SMessageId] = RpcService{"ClientPlayerSceneService", "SceneInfoC2S", std::make_unique_for_overwrite<::SceneInfoRequest>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<ClientPlayerSceneServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceByMessageId[ClientPlayerSceneServiceNotifySceneInfoMessageId] = RpcService{"ClientPlayerSceneService", "NotifySceneInfo", std::make_unique_for_overwrite<::SceneInfoS2C>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<ClientPlayerSceneServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceByMessageId[ClientPlayerSceneServiceNotifyActorCreateMessageId] = RpcService{"ClientPlayerSceneService", "NotifyActorCreate", std::make_unique_for_overwrite<::ActorCreateS2C>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<ClientPlayerSceneServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceByMessageId[ClientPlayerSceneServiceNotifyActorDestroyMessageId] = RpcService{"ClientPlayerSceneService", "NotifyActorDestroy", std::make_unique_for_overwrite<::ActorDestroyS2C>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<ClientPlayerSceneServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceByMessageId[ClientPlayerSceneServiceNotifyActorListCreateMessageId] = RpcService{"ClientPlayerSceneService", "NotifyActorListCreate", std::make_unique_for_overwrite<::ActorListCreateS2C>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<ClientPlayerSceneServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceByMessageId[ClientPlayerSceneServiceNotifyActorListDestroyMessageId] = RpcService{"ClientPlayerSceneService", "NotifyActorListDestroy", std::make_unique_for_overwrite<::ActorListDestroyS2C>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<ClientPlayerSceneServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceByMessageId[ClientPlayerSkillServiceReleaseSkillMessageId] = RpcService{"ClientPlayerSkillService", "ReleaseSkill", std::make_unique_for_overwrite<::ReleaseSkillSkillRequest>(), std::make_unique_for_overwrite<::ReleaseSkillSkillResponse>(), std::make_unique_for_overwrite<ClientPlayerSkillServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceByMessageId[ClientPlayerSkillServiceNotifySkillUsedMessageId] = RpcService{"ClientPlayerSkillService", "NotifySkillUsed", std::make_unique_for_overwrite<::SkillUsedS2C>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<ClientPlayerSkillServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceByMessageId[ClientPlayerSkillServiceNotifySkillInterruptedMessageId] = RpcService{"ClientPlayerSkillService", "NotifySkillInterrupted", std::make_unique_for_overwrite<::SkillInterruptedS2C>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<ClientPlayerSkillServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceByMessageId[ClientPlayerSkillServiceGetSkillListMessageId] = RpcService{"ClientPlayerSkillService", "GetSkillList", std::make_unique_for_overwrite<::GetSkillListRequest>(), std::make_unique_for_overwrite<::GetSkillListResponse>(), std::make_unique_for_overwrite<ClientPlayerSkillServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceByMessageId[EntitySyncServiceSyncBaseAttributeMessageId] = RpcService{"EntitySyncService", "SyncBaseAttribute", std::make_unique_for_overwrite<::BaseAttributeSyncDataS2C>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<EntitySyncServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceByMessageId[EntitySyncServiceSyncAttribute2FramesMessageId] = RpcService{"EntitySyncService", "SyncAttribute2Frames", std::make_unique_for_overwrite<::AttributeDelta2FramesS2C>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<EntitySyncServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceByMessageId[EntitySyncServiceSyncAttribute5FramesMessageId] = RpcService{"EntitySyncService", "SyncAttribute5Frames", std::make_unique_for_overwrite<::AttributeDelta5FramesS2C>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<EntitySyncServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceByMessageId[EntitySyncServiceSyncAttribute10FramesMessageId] = RpcService{"EntitySyncService", "SyncAttribute10Frames", std::make_unique_for_overwrite<::AttributeDelta10FramesS2C>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<EntitySyncServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceByMessageId[EntitySyncServiceSyncAttribute30FramesMessageId] = RpcService{"EntitySyncService", "SyncAttribute30Frames", std::make_unique_for_overwrite<::AttributeDelta30FramesS2C>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<EntitySyncServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceByMessageId[EntitySyncServiceSyncAttribute60FramesMessageId] = RpcService{"EntitySyncService", "SyncAttribute60Frames", std::make_unique_for_overwrite<::AttributeDelta60FramesS2C>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<EntitySyncServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceByMessageId[GameClientPlayerCommonServiceSendTipToClientMessageId] = RpcService{"GameClientPlayerCommonService", "SendTipToClient", std::make_unique_for_overwrite<::TipInfoMessage>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<GameClientPlayerCommonServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceByMessageId[GameClientPlayerCommonServiceKickPlayerMessageId] = RpcService{"GameClientPlayerCommonService", "KickPlayer", std::make_unique_for_overwrite<::GameKickPlayerRequest>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<GameClientPlayerCommonServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceByMessageId[GamePlayerSceneServiceEnterSceneMessageId] = RpcService{"GamePlayerSceneService", "EnterScene", std::make_unique_for_overwrite<::GsEnterSceneRequest>(), std::make_unique_for_overwrite<::google::protobuf::Empty>(), std::make_unique_for_overwrite<GamePlayerSceneServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceByMessageId[GamePlayerSceneServiceLeaveSceneMessageId] = RpcService{"GamePlayerSceneService", "LeaveScene", std::make_unique_for_overwrite<::GsLeaveSceneRequest>(), std::make_unique_for_overwrite<::google::protobuf::Empty>(), std::make_unique_for_overwrite<GamePlayerSceneServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceByMessageId[GamePlayerSceneServiceEnterSceneS2CMessageId] = RpcService{"GamePlayerSceneService", "EnterSceneS2C", std::make_unique_for_overwrite<::EnterSceneS2CRequest>(), std::make_unique_for_overwrite<::EnterScenerS2CResponse>(), std::make_unique_for_overwrite<GamePlayerSceneServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceByMessageId[GamePlayerServiceCentre2GsLoginMessageId] = RpcService{"GamePlayerService", "Centre2GsLogin", std::make_unique_for_overwrite<::Centre2GsLoginRequest>(), std::make_unique_for_overwrite<::google::protobuf::Empty>(), std::make_unique_for_overwrite<GamePlayerServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceByMessageId[GamePlayerServiceExitGameMessageId] = RpcService{"GamePlayerService", "ExitGame", std::make_unique_for_overwrite<::GameNodeExitGameRequest>(), std::make_unique_for_overwrite<::google::protobuf::Empty>(), std::make_unique_for_overwrite<GamePlayerServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceByMessageId[GameSceneServiceTestMessageId] = RpcService{"GameSceneService", "Test", std::make_unique_for_overwrite<::GameSceneTest>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<GameSceneServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceByMessageId[GameServicePlayerEnterGameNodeMessageId] = RpcService{"GameService", "PlayerEnterGameNode", std::make_unique_for_overwrite<::PlayerEnterGameNodeRequest>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<GameServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceByMessageId[GameServiceSendMessageToPlayerMessageId] = RpcService{"GameService", "SendMessageToPlayer", std::make_unique_for_overwrite<::NodeRouteMessageRequest>(), std::make_unique_for_overwrite<::NodeRouteMessageResponse>(), std::make_unique_for_overwrite<GameServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceByMessageId[GameServiceClientSendMessageToPlayerMessageId] = RpcService{"GameService", "ClientSendMessageToPlayer", std::make_unique_for_overwrite<::ClientSendMessageToPlayerRequest>(), std::make_unique_for_overwrite<::ClientSendMessageToPlayerResponse>(), std::make_unique_for_overwrite<GameServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceByMessageId[GameServiceCentreSendToPlayerViaGameNodeMessageId] = RpcService{"GameService", "CentreSendToPlayerViaGameNode", std::make_unique_for_overwrite<::NodeRouteMessageRequest>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<GameServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceByMessageId[GameServiceInvokePlayerServiceMessageId] = RpcService{"GameService", "InvokePlayerService", std::make_unique_for_overwrite<::NodeRouteMessageRequest>(), std::make_unique_for_overwrite<::NodeRouteMessageResponse>(), std::make_unique_for_overwrite<GameServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceByMessageId[GameServiceRouteNodeStringMsgMessageId] = RpcService{"GameService", "RouteNodeStringMsg", std::make_unique_for_overwrite<::RouteMessageRequest>(), std::make_unique_for_overwrite<::RouteMessageResponse>(), std::make_unique_for_overwrite<GameServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceByMessageId[GameServiceRoutePlayerStringMsgMessageId] = RpcService{"GameService", "RoutePlayerStringMsg", std::make_unique_for_overwrite<::RoutePlayerMessageRequest>(), std::make_unique_for_overwrite<::RoutePlayerMessageResponse>(), std::make_unique_for_overwrite<GameServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceByMessageId[GameServiceUpdateSessionDetailMessageId] = RpcService{"GameService", "UpdateSessionDetail", std::make_unique_for_overwrite<::RegisterPlayerSessionRequest>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<GameServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceByMessageId[GameServiceEnterSceneMessageId] = RpcService{"GameService", "EnterScene", std::make_unique_for_overwrite<::Centre2GsEnterSceneRequest>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<GameServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceByMessageId[GameServiceCreateSceneMessageId] = RpcService{"GameService", "CreateScene", std::make_unique_for_overwrite<::CreateSceneRequest>(), std::make_unique_for_overwrite<::CreateSceneResponse>(), std::make_unique_for_overwrite<GameServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceByMessageId[GameServiceRegisterNodeSessionMessageId] = RpcService{"GameService", "RegisterNodeSession", std::make_unique_for_overwrite<::RegisterNodeSessionRequest>(), std::make_unique_for_overwrite<::RegisterNodeSessionResponse>(), std::make_unique_for_overwrite<GameServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceByMessageId[GateServicePlayerEnterGameNodeMessageId] = RpcService{"GateService", "PlayerEnterGameNode", std::make_unique_for_overwrite<::RegisterGameNodeSessionRequest>(), std::make_unique_for_overwrite<::RegisterGameNodeSessionResponse>(), std::make_unique_for_overwrite<GateServiceImpl>(), 0, eNodeType::GateNodeService};
    gRpcServiceByMessageId[GateServiceSendMessageToPlayerMessageId] = RpcService{"GateService", "SendMessageToPlayer", std::make_unique_for_overwrite<::NodeRouteMessageRequest>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<GateServiceImpl>(), 0, eNodeType::GateNodeService};
    gRpcServiceByMessageId[GateServiceKickSessionByCentreMessageId] = RpcService{"GateService", "KickSessionByCentre", std::make_unique_for_overwrite<::KickSessionRequest>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<GateServiceImpl>(), 0, eNodeType::GateNodeService};
    gRpcServiceByMessageId[GateServiceRouteNodeMessageMessageId] = RpcService{"GateService", "RouteNodeMessage", std::make_unique_for_overwrite<::RouteMessageRequest>(), std::make_unique_for_overwrite<::RouteMessageResponse>(), std::make_unique_for_overwrite<GateServiceImpl>(), 0, eNodeType::GateNodeService};
    gRpcServiceByMessageId[GateServiceRoutePlayerMessageMessageId] = RpcService{"GateService", "RoutePlayerMessage", std::make_unique_for_overwrite<::RoutePlayerMessageRequest>(), std::make_unique_for_overwrite<::RoutePlayerMessageResponse>(), std::make_unique_for_overwrite<GateServiceImpl>(), 0, eNodeType::GateNodeService};
    gRpcServiceByMessageId[GateServiceBroadcastToPlayersMessageId] = RpcService{"GateService", "BroadcastToPlayers", std::make_unique_for_overwrite<::BroadcastToPlayersRequest>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<GateServiceImpl>(), 0, eNodeType::GateNodeService};
    gRpcServiceByMessageId[GateServiceRegisterNodeSessionMessageId] = RpcService{"GateService", "RegisterNodeSession", std::make_unique_for_overwrite<::RegisterNodeSessionRequest>(), std::make_unique_for_overwrite<::RegisterNodeSessionResponse>(), std::make_unique_for_overwrite<GateServiceImpl>(), 0, eNodeType::GateNodeService};
    gRpcServiceByMessageId[KVRangeMessageId] = RpcService{"KV", "Range", std::make_unique_for_overwrite<::etcdserverpb::RangeRequest>(), std::make_unique_for_overwrite<::etcdserverpb::RangeResponse>(), nullptr, 0, eNodeType::EtcdNodeService};
    gRpcServiceByMessageId[KVPutMessageId] = RpcService{"KV", "Put", std::make_unique_for_overwrite<::etcdserverpb::PutRequest>(), std::make_unique_for_overwrite<::etcdserverpb::PutResponse>(), nullptr, 0, eNodeType::EtcdNodeService};
    gRpcServiceByMessageId[KVDeleteRangeMessageId] = RpcService{"KV", "DeleteRange", std::make_unique_for_overwrite<::etcdserverpb::DeleteRangeRequest>(), std::make_unique_for_overwrite<::etcdserverpb::DeleteRangeResponse>(), nullptr, 0, eNodeType::EtcdNodeService};
    gRpcServiceByMessageId[KVTxnMessageId] = RpcService{"KV", "Txn", std::make_unique_for_overwrite<::etcdserverpb::TxnRequest>(), std::make_unique_for_overwrite<::etcdserverpb::TxnResponse>(), nullptr, 0, eNodeType::EtcdNodeService};
    gRpcServiceByMessageId[KVCompactMessageId] = RpcService{"KV", "Compact", std::make_unique_for_overwrite<::etcdserverpb::CompactionRequest>(), std::make_unique_for_overwrite<::etcdserverpb::CompactionResponse>(), nullptr, 0, eNodeType::EtcdNodeService};
    gRpcServiceByMessageId[LeaseLeaseGrantMessageId] = RpcService{"Lease", "LeaseGrant", std::make_unique_for_overwrite<::etcdserverpb::LeaseGrantRequest>(), std::make_unique_for_overwrite<::etcdserverpb::LeaseGrantResponse>(), nullptr, 0, eNodeType::EtcdNodeService};
    gRpcServiceByMessageId[LeaseLeaseRevokeMessageId] = RpcService{"Lease", "LeaseRevoke", std::make_unique_for_overwrite<::etcdserverpb::LeaseRevokeRequest>(), std::make_unique_for_overwrite<::etcdserverpb::LeaseRevokeResponse>(), nullptr, 0, eNodeType::EtcdNodeService};
    gRpcServiceByMessageId[LeaseLeaseKeepAliveMessageId] = RpcService{"Lease", "LeaseKeepAlive", std::make_unique_for_overwrite<::etcdserverpb::LeaseKeepAliveRequest>(), std::make_unique_for_overwrite<::etcdserverpb::LeaseKeepAliveResponse>(), nullptr, 0, eNodeType::EtcdNodeService};
    gRpcServiceByMessageId[LeaseLeaseTimeToLiveMessageId] = RpcService{"Lease", "LeaseTimeToLive", std::make_unique_for_overwrite<::etcdserverpb::LeaseTimeToLiveRequest>(), std::make_unique_for_overwrite<::etcdserverpb::LeaseTimeToLiveResponse>(), nullptr, 0, eNodeType::EtcdNodeService};
    gRpcServiceByMessageId[LeaseLeaseLeasesMessageId] = RpcService{"Lease", "LeaseLeases", std::make_unique_for_overwrite<::etcdserverpb::LeaseLeasesRequest>(), std::make_unique_for_overwrite<::etcdserverpb::LeaseLeasesResponse>(), nullptr, 0, eNodeType::EtcdNodeService};
    gRpcServiceByMessageId[LoginServiceLoginMessageId] = RpcService{"LoginService", "Login", std::make_unique_for_overwrite<::loginpb::LoginC2LRequest>(), std::make_unique_for_overwrite<::loginpb::LoginC2LResponse>(), nullptr, 1, eNodeType::LoginNodeService};
    gRpcServiceByMessageId[LoginServiceCreatePlayerMessageId] = RpcService{"LoginService", "CreatePlayer", std::make_unique_for_overwrite<::loginpb::CreatePlayerC2LRequest>(), std::make_unique_for_overwrite<::loginpb::CreatePlayerC2LResponse>(), nullptr, 1, eNodeType::LoginNodeService};
    gRpcServiceByMessageId[LoginServiceEnterGameMessageId] = RpcService{"LoginService", "EnterGame", std::make_unique_for_overwrite<::loginpb::EnterGameC2LRequest>(), std::make_unique_for_overwrite<::loginpb::EnterGameC2LResponse>(), nullptr, 1, eNodeType::LoginNodeService};
    gRpcServiceByMessageId[LoginServiceLeaveGameMessageId] = RpcService{"LoginService", "LeaveGame", std::make_unique_for_overwrite<::loginpb::LeaveGameC2LRequest>(), std::make_unique_for_overwrite<::Empty>(), nullptr, 1, eNodeType::LoginNodeService};
    gRpcServiceByMessageId[LoginServiceDisconnectMessageId] = RpcService{"LoginService", "Disconnect", std::make_unique_for_overwrite<::loginpb::LoginNodeDisconnectRequest>(), std::make_unique_for_overwrite<::Empty>(), nullptr, 1, eNodeType::LoginNodeService};
    gRpcServiceByMessageId[PlayerDBServiceLoad2RedisMessageId] = RpcService{"PlayerDBService", "Load2Redis", std::make_unique_for_overwrite<::LoadPlayerRequest>(), std::make_unique_for_overwrite<::LoadPlayerResponse>(), nullptr, 0, eNodeType::DbNodeService};
    gRpcServiceByMessageId[PlayerDBServiceSave2RedisMessageId] = RpcService{"PlayerDBService", "Save2Redis", std::make_unique_for_overwrite<::SavePlayerRequest>(), std::make_unique_for_overwrite<::SavePlayerResponse>(), nullptr, 0, eNodeType::DbNodeService};
    gRpcServiceByMessageId[WatchWatchMessageId] = RpcService{"Watch", "Watch", std::make_unique_for_overwrite<::etcdserverpb::WatchRequest>(), std::make_unique_for_overwrite<::etcdserverpb::WatchResponse>(), nullptr, 0, eNodeType::EtcdNodeService};
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
