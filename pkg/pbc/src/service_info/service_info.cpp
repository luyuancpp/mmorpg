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

namespace {void SendAccountDBServiceLoad2Redis(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace {void SendAccountDBServiceSave2Redis(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace etcdserverpb{void SendKVRange(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace etcdserverpb{void SendKVPut(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace etcdserverpb{void SendKVDeleteRange(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace etcdserverpb{void SendKVTxn(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace etcdserverpb{void SendKVCompact(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace etcdserverpb{void SendLeaseLeaseGrant(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace etcdserverpb{void SendLeaseLeaseRevoke(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace etcdserverpb{void SendLeaseLeaseKeepAlive(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace etcdserverpb{void SendLeaseLeaseTimeToLive(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace etcdserverpb{void SendLeaseLeaseLeases(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace loginpb{void SendLoginServiceLogin(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace loginpb{void SendLoginServiceCreatePlayer(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace loginpb{void SendLoginServiceEnterGame(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace loginpb{void SendLoginServiceLeaveGame(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace loginpb{void SendLoginServiceDisconnect(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace {void SendPlayerDBServiceLoad2Redis(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace {void SendPlayerDBServiceSave2Redis(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace etcdserverpb{void SendWatchWatch(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}

std::unordered_set<uint32_t> gClientMessageIdWhitelist;
std::array<RpcService, 85> gRpcServiceRegistry;

void InitMessageInfo()
{
    gRpcServiceRegistry[AccountDBServiceLoad2RedisMessageId] = RpcService{"AccountDBService", "Load2Redis", std::make_unique_for_overwrite<::LoadAccountRequest>(), std::make_unique_for_overwrite<::LoadAccountResponse>(), nullptr, 0, eNodeType::DbNodeService, ::SendAccountDBServiceLoad2Redis};
    gRpcServiceRegistry[AccountDBServiceSave2RedisMessageId] = RpcService{"AccountDBService", "Save2Redis", std::make_unique_for_overwrite<::SaveAccountRequest>(), std::make_unique_for_overwrite<::SaveAccountResponse>(), nullptr, 0, eNodeType::DbNodeService, ::SendAccountDBServiceSave2Redis};
    gRpcServiceRegistry[CentreClientPlayerCommonServiceSendTipToClientMessageId] = RpcService{"CentreClientPlayerCommonService", "SendTipToClient", std::make_unique_for_overwrite<::TipInfoMessage>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<CentreClientPlayerCommonServiceImpl>(), 0, eNodeType::CentreNodeService};
    gRpcServiceRegistry[CentreClientPlayerCommonServiceKickPlayerMessageId] = RpcService{"CentreClientPlayerCommonService", "KickPlayer", std::make_unique_for_overwrite<::CentreKickPlayerRequest>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<CentreClientPlayerCommonServiceImpl>(), 0, eNodeType::CentreNodeService};
    gRpcServiceRegistry[CentrePlayerSceneServiceEnterSceneMessageId] = RpcService{"CentrePlayerSceneService", "EnterScene", std::make_unique_for_overwrite<::CentreEnterSceneRequest>(), std::make_unique_for_overwrite<::google::protobuf::Empty>(), std::make_unique_for_overwrite<CentrePlayerSceneServiceImpl>(), 0, eNodeType::CentreNodeService};
    gRpcServiceRegistry[CentrePlayerSceneServiceLeaveSceneMessageId] = RpcService{"CentrePlayerSceneService", "LeaveScene", std::make_unique_for_overwrite<::CentreLeaveSceneRequest>(), std::make_unique_for_overwrite<::google::protobuf::Empty>(), std::make_unique_for_overwrite<CentrePlayerSceneServiceImpl>(), 0, eNodeType::CentreNodeService};
    gRpcServiceRegistry[CentrePlayerSceneServiceLeaveSceneAsyncSavePlayerCompleteMessageId] = RpcService{"CentrePlayerSceneService", "LeaveSceneAsyncSavePlayerComplete", std::make_unique_for_overwrite<::CentreLeaveSceneAsyncSavePlayerCompleteRequest>(), std::make_unique_for_overwrite<::google::protobuf::Empty>(), std::make_unique_for_overwrite<CentrePlayerSceneServiceImpl>(), 0, eNodeType::CentreNodeService};
    gRpcServiceRegistry[CentrePlayerSceneServiceSceneInfoC2SMessageId] = RpcService{"CentrePlayerSceneService", "SceneInfoC2S", std::make_unique_for_overwrite<::CentreSceneInfoRequest>(), std::make_unique_for_overwrite<::google::protobuf::Empty>(), std::make_unique_for_overwrite<CentrePlayerSceneServiceImpl>(), 0, eNodeType::CentreNodeService};
    gRpcServiceRegistry[CentrePlayerServiceTestMessageId] = RpcService{"CentrePlayerService", "Test", std::make_unique_for_overwrite<::google::protobuf::Empty>(), std::make_unique_for_overwrite<::google::protobuf::Empty>(), std::make_unique_for_overwrite<CentrePlayerServiceImpl>(), 0, eNodeType::CentreNodeService};
    gRpcServiceRegistry[CentreSceneServiceRegisterSceneMessageId] = RpcService{"CentreSceneService", "RegisterScene", std::make_unique_for_overwrite<::RegisterSceneRequest>(), std::make_unique_for_overwrite<::RegisterSceneResponse>(), std::make_unique_for_overwrite<CentreSceneServiceImpl>(), 0, eNodeType::CentreNodeService};
    gRpcServiceRegistry[CentreSceneServiceUnRegisterSceneMessageId] = RpcService{"CentreSceneService", "UnRegisterScene", std::make_unique_for_overwrite<::UnRegisterSceneRequest>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<CentreSceneServiceImpl>(), 0, eNodeType::CentreNodeService};
    gRpcServiceRegistry[CentreServiceGatePlayerServiceMessageId] = RpcService{"CentreService", "GatePlayerService", std::make_unique_for_overwrite<::GateClientMessageRequest>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<CentreServiceImpl>(), 0, eNodeType::CentreNodeService};
    gRpcServiceRegistry[CentreServiceGateSessionDisconnectMessageId] = RpcService{"CentreService", "GateSessionDisconnect", std::make_unique_for_overwrite<::GateSessionDisconnectRequest>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<CentreServiceImpl>(), 0, eNodeType::CentreNodeService};
    gRpcServiceRegistry[CentreServiceLoginNodeAccountLoginMessageId] = RpcService{"CentreService", "LoginNodeAccountLogin", std::make_unique_for_overwrite<::LoginRequest>(), std::make_unique_for_overwrite<::LoginResponse>(), std::make_unique_for_overwrite<CentreServiceImpl>(), 0, eNodeType::CentreNodeService};
    gRpcServiceRegistry[CentreServiceLoginNodeEnterGameMessageId] = RpcService{"CentreService", "LoginNodeEnterGame", std::make_unique_for_overwrite<::CentrePlayerGameNodeEntryRequest>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<CentreServiceImpl>(), 0, eNodeType::CentreNodeService};
    gRpcServiceRegistry[CentreServiceLoginNodeLeaveGameMessageId] = RpcService{"CentreService", "LoginNodeLeaveGame", std::make_unique_for_overwrite<::LoginNodeLeaveGameRequest>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<CentreServiceImpl>(), 0, eNodeType::CentreNodeService};
    gRpcServiceRegistry[CentreServiceLoginNodeSessionDisconnectMessageId] = RpcService{"CentreService", "LoginNodeSessionDisconnect", std::make_unique_for_overwrite<::GateSessionDisconnectRequest>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<CentreServiceImpl>(), 0, eNodeType::CentreNodeService};
    gRpcServiceRegistry[CentreServicePlayerServiceMessageId] = RpcService{"CentreService", "PlayerService", std::make_unique_for_overwrite<::NodeRouteMessageRequest>(), std::make_unique_for_overwrite<::NodeRouteMessageResponse>(), std::make_unique_for_overwrite<CentreServiceImpl>(), 0, eNodeType::CentreNodeService};
    gRpcServiceRegistry[CentreServiceEnterGsSucceedMessageId] = RpcService{"CentreService", "EnterGsSucceed", std::make_unique_for_overwrite<::EnterGameNodeSuccessRequest>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<CentreServiceImpl>(), 0, eNodeType::CentreNodeService};
    gRpcServiceRegistry[CentreServiceRouteNodeStringMsgMessageId] = RpcService{"CentreService", "RouteNodeStringMsg", std::make_unique_for_overwrite<::RouteMessageRequest>(), std::make_unique_for_overwrite<::RouteMessageResponse>(), std::make_unique_for_overwrite<CentreServiceImpl>(), 0, eNodeType::CentreNodeService};
    gRpcServiceRegistry[CentreServiceRoutePlayerStringMsgMessageId] = RpcService{"CentreService", "RoutePlayerStringMsg", std::make_unique_for_overwrite<::RoutePlayerMessageRequest>(), std::make_unique_for_overwrite<::RoutePlayerMessageResponse>(), std::make_unique_for_overwrite<CentreServiceImpl>(), 0, eNodeType::CentreNodeService};
    gRpcServiceRegistry[CentreServiceInitSceneNodeMessageId] = RpcService{"CentreService", "InitSceneNode", std::make_unique_for_overwrite<::InitSceneNodeRequest>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<CentreServiceImpl>(), 0, eNodeType::CentreNodeService};
    gRpcServiceRegistry[CentreServiceRegisterNodeSessionMessageId] = RpcService{"CentreService", "RegisterNodeSession", std::make_unique_for_overwrite<::RegisterNodeSessionRequest>(), std::make_unique_for_overwrite<::RegisterNodeSessionResponse>(), std::make_unique_for_overwrite<CentreServiceImpl>(), 0, eNodeType::CentreNodeService};
    gRpcServiceRegistry[ClientPlayerSceneServiceEnterSceneMessageId] = RpcService{"ClientPlayerSceneService", "EnterScene", std::make_unique_for_overwrite<::EnterSceneC2SRequest>(), std::make_unique_for_overwrite<::EnterSceneC2SResponse>(), std::make_unique_for_overwrite<ClientPlayerSceneServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[ClientPlayerSceneServiceNotifyEnterSceneMessageId] = RpcService{"ClientPlayerSceneService", "NotifyEnterScene", std::make_unique_for_overwrite<::EnterSceneS2C>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<ClientPlayerSceneServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[ClientPlayerSceneServiceSceneInfoC2SMessageId] = RpcService{"ClientPlayerSceneService", "SceneInfoC2S", std::make_unique_for_overwrite<::SceneInfoRequest>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<ClientPlayerSceneServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[ClientPlayerSceneServiceNotifySceneInfoMessageId] = RpcService{"ClientPlayerSceneService", "NotifySceneInfo", std::make_unique_for_overwrite<::SceneInfoS2C>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<ClientPlayerSceneServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[ClientPlayerSceneServiceNotifyActorCreateMessageId] = RpcService{"ClientPlayerSceneService", "NotifyActorCreate", std::make_unique_for_overwrite<::ActorCreateS2C>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<ClientPlayerSceneServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[ClientPlayerSceneServiceNotifyActorDestroyMessageId] = RpcService{"ClientPlayerSceneService", "NotifyActorDestroy", std::make_unique_for_overwrite<::ActorDestroyS2C>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<ClientPlayerSceneServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[ClientPlayerSceneServiceNotifyActorListCreateMessageId] = RpcService{"ClientPlayerSceneService", "NotifyActorListCreate", std::make_unique_for_overwrite<::ActorListCreateS2C>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<ClientPlayerSceneServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[ClientPlayerSceneServiceNotifyActorListDestroyMessageId] = RpcService{"ClientPlayerSceneService", "NotifyActorListDestroy", std::make_unique_for_overwrite<::ActorListDestroyS2C>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<ClientPlayerSceneServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[ClientPlayerSkillServiceReleaseSkillMessageId] = RpcService{"ClientPlayerSkillService", "ReleaseSkill", std::make_unique_for_overwrite<::ReleaseSkillSkillRequest>(), std::make_unique_for_overwrite<::ReleaseSkillSkillResponse>(), std::make_unique_for_overwrite<ClientPlayerSkillServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[ClientPlayerSkillServiceNotifySkillUsedMessageId] = RpcService{"ClientPlayerSkillService", "NotifySkillUsed", std::make_unique_for_overwrite<::SkillUsedS2C>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<ClientPlayerSkillServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[ClientPlayerSkillServiceNotifySkillInterruptedMessageId] = RpcService{"ClientPlayerSkillService", "NotifySkillInterrupted", std::make_unique_for_overwrite<::SkillInterruptedS2C>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<ClientPlayerSkillServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[ClientPlayerSkillServiceGetSkillListMessageId] = RpcService{"ClientPlayerSkillService", "GetSkillList", std::make_unique_for_overwrite<::GetSkillListRequest>(), std::make_unique_for_overwrite<::GetSkillListResponse>(), std::make_unique_for_overwrite<ClientPlayerSkillServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[EntitySyncServiceSyncBaseAttributeMessageId] = RpcService{"EntitySyncService", "SyncBaseAttribute", std::make_unique_for_overwrite<::BaseAttributeSyncDataS2C>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<EntitySyncServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[EntitySyncServiceSyncAttribute2FramesMessageId] = RpcService{"EntitySyncService", "SyncAttribute2Frames", std::make_unique_for_overwrite<::AttributeDelta2FramesS2C>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<EntitySyncServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[EntitySyncServiceSyncAttribute5FramesMessageId] = RpcService{"EntitySyncService", "SyncAttribute5Frames", std::make_unique_for_overwrite<::AttributeDelta5FramesS2C>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<EntitySyncServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[EntitySyncServiceSyncAttribute10FramesMessageId] = RpcService{"EntitySyncService", "SyncAttribute10Frames", std::make_unique_for_overwrite<::AttributeDelta10FramesS2C>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<EntitySyncServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[EntitySyncServiceSyncAttribute30FramesMessageId] = RpcService{"EntitySyncService", "SyncAttribute30Frames", std::make_unique_for_overwrite<::AttributeDelta30FramesS2C>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<EntitySyncServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[EntitySyncServiceSyncAttribute60FramesMessageId] = RpcService{"EntitySyncService", "SyncAttribute60Frames", std::make_unique_for_overwrite<::AttributeDelta60FramesS2C>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<EntitySyncServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[GameClientPlayerCommonServiceSendTipToClientMessageId] = RpcService{"GameClientPlayerCommonService", "SendTipToClient", std::make_unique_for_overwrite<::TipInfoMessage>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<GameClientPlayerCommonServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[GameClientPlayerCommonServiceKickPlayerMessageId] = RpcService{"GameClientPlayerCommonService", "KickPlayer", std::make_unique_for_overwrite<::GameKickPlayerRequest>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<GameClientPlayerCommonServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[GamePlayerSceneServiceEnterSceneMessageId] = RpcService{"GamePlayerSceneService", "EnterScene", std::make_unique_for_overwrite<::GsEnterSceneRequest>(), std::make_unique_for_overwrite<::google::protobuf::Empty>(), std::make_unique_for_overwrite<GamePlayerSceneServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[GamePlayerSceneServiceLeaveSceneMessageId] = RpcService{"GamePlayerSceneService", "LeaveScene", std::make_unique_for_overwrite<::GsLeaveSceneRequest>(), std::make_unique_for_overwrite<::google::protobuf::Empty>(), std::make_unique_for_overwrite<GamePlayerSceneServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[GamePlayerSceneServiceEnterSceneS2CMessageId] = RpcService{"GamePlayerSceneService", "EnterSceneS2C", std::make_unique_for_overwrite<::EnterSceneS2CRequest>(), std::make_unique_for_overwrite<::EnterScenerS2CResponse>(), std::make_unique_for_overwrite<GamePlayerSceneServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[GamePlayerServiceCentre2GsLoginMessageId] = RpcService{"GamePlayerService", "Centre2GsLogin", std::make_unique_for_overwrite<::Centre2GsLoginRequest>(), std::make_unique_for_overwrite<::google::protobuf::Empty>(), std::make_unique_for_overwrite<GamePlayerServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[GamePlayerServiceExitGameMessageId] = RpcService{"GamePlayerService", "ExitGame", std::make_unique_for_overwrite<::GameNodeExitGameRequest>(), std::make_unique_for_overwrite<::google::protobuf::Empty>(), std::make_unique_for_overwrite<GamePlayerServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[GameSceneServiceTestMessageId] = RpcService{"GameSceneService", "Test", std::make_unique_for_overwrite<::GameSceneTest>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<GameSceneServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[GameServicePlayerEnterGameNodeMessageId] = RpcService{"GameService", "PlayerEnterGameNode", std::make_unique_for_overwrite<::PlayerEnterGameNodeRequest>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<GameServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[GameServiceSendMessageToPlayerMessageId] = RpcService{"GameService", "SendMessageToPlayer", std::make_unique_for_overwrite<::NodeRouteMessageRequest>(), std::make_unique_for_overwrite<::NodeRouteMessageResponse>(), std::make_unique_for_overwrite<GameServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[GameServiceClientSendMessageToPlayerMessageId] = RpcService{"GameService", "ClientSendMessageToPlayer", std::make_unique_for_overwrite<::ClientSendMessageToPlayerRequest>(), std::make_unique_for_overwrite<::ClientSendMessageToPlayerResponse>(), std::make_unique_for_overwrite<GameServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[GameServiceCentreSendToPlayerViaGameNodeMessageId] = RpcService{"GameService", "CentreSendToPlayerViaGameNode", std::make_unique_for_overwrite<::NodeRouteMessageRequest>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<GameServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[GameServiceInvokePlayerServiceMessageId] = RpcService{"GameService", "InvokePlayerService", std::make_unique_for_overwrite<::NodeRouteMessageRequest>(), std::make_unique_for_overwrite<::NodeRouteMessageResponse>(), std::make_unique_for_overwrite<GameServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[GameServiceRouteNodeStringMsgMessageId] = RpcService{"GameService", "RouteNodeStringMsg", std::make_unique_for_overwrite<::RouteMessageRequest>(), std::make_unique_for_overwrite<::RouteMessageResponse>(), std::make_unique_for_overwrite<GameServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[GameServiceRoutePlayerStringMsgMessageId] = RpcService{"GameService", "RoutePlayerStringMsg", std::make_unique_for_overwrite<::RoutePlayerMessageRequest>(), std::make_unique_for_overwrite<::RoutePlayerMessageResponse>(), std::make_unique_for_overwrite<GameServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[GameServiceUpdateSessionDetailMessageId] = RpcService{"GameService", "UpdateSessionDetail", std::make_unique_for_overwrite<::RegisterPlayerSessionRequest>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<GameServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[GameServiceEnterSceneMessageId] = RpcService{"GameService", "EnterScene", std::make_unique_for_overwrite<::Centre2GsEnterSceneRequest>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<GameServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[GameServiceCreateSceneMessageId] = RpcService{"GameService", "CreateScene", std::make_unique_for_overwrite<::CreateSceneRequest>(), std::make_unique_for_overwrite<::CreateSceneResponse>(), std::make_unique_for_overwrite<GameServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[GameServiceRegisterNodeSessionMessageId] = RpcService{"GameService", "RegisterNodeSession", std::make_unique_for_overwrite<::RegisterNodeSessionRequest>(), std::make_unique_for_overwrite<::RegisterNodeSessionResponse>(), std::make_unique_for_overwrite<GameServiceImpl>(), 0, eNodeType::SceneNodeService};
    gRpcServiceRegistry[GateServicePlayerEnterGameNodeMessageId] = RpcService{"GateService", "PlayerEnterGameNode", std::make_unique_for_overwrite<::RegisterGameNodeSessionRequest>(), std::make_unique_for_overwrite<::RegisterGameNodeSessionResponse>(), std::make_unique_for_overwrite<GateServiceImpl>(), 0, eNodeType::GateNodeService};
    gRpcServiceRegistry[GateServiceSendMessageToPlayerMessageId] = RpcService{"GateService", "SendMessageToPlayer", std::make_unique_for_overwrite<::NodeRouteMessageRequest>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<GateServiceImpl>(), 0, eNodeType::GateNodeService};
    gRpcServiceRegistry[GateServiceKickSessionByCentreMessageId] = RpcService{"GateService", "KickSessionByCentre", std::make_unique_for_overwrite<::KickSessionRequest>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<GateServiceImpl>(), 0, eNodeType::GateNodeService};
    gRpcServiceRegistry[GateServiceRouteNodeMessageMessageId] = RpcService{"GateService", "RouteNodeMessage", std::make_unique_for_overwrite<::RouteMessageRequest>(), std::make_unique_for_overwrite<::RouteMessageResponse>(), std::make_unique_for_overwrite<GateServiceImpl>(), 0, eNodeType::GateNodeService};
    gRpcServiceRegistry[GateServiceRoutePlayerMessageMessageId] = RpcService{"GateService", "RoutePlayerMessage", std::make_unique_for_overwrite<::RoutePlayerMessageRequest>(), std::make_unique_for_overwrite<::RoutePlayerMessageResponse>(), std::make_unique_for_overwrite<GateServiceImpl>(), 0, eNodeType::GateNodeService};
    gRpcServiceRegistry[GateServiceBroadcastToPlayersMessageId] = RpcService{"GateService", "BroadcastToPlayers", std::make_unique_for_overwrite<::BroadcastToPlayersRequest>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<GateServiceImpl>(), 0, eNodeType::GateNodeService};
    gRpcServiceRegistry[GateServiceRegisterNodeSessionMessageId] = RpcService{"GateService", "RegisterNodeSession", std::make_unique_for_overwrite<::RegisterNodeSessionRequest>(), std::make_unique_for_overwrite<::RegisterNodeSessionResponse>(), std::make_unique_for_overwrite<GateServiceImpl>(), 0, eNodeType::GateNodeService};
    gRpcServiceRegistry[KVRangeMessageId] = RpcService{"KV", "Range", std::make_unique_for_overwrite<::etcdserverpb::RangeRequest>(), std::make_unique_for_overwrite<::etcdserverpb::RangeResponse>(), nullptr, 0, eNodeType::EtcdNodeService, etcdserverpb::SendKVRange};
    gRpcServiceRegistry[KVPutMessageId] = RpcService{"KV", "Put", std::make_unique_for_overwrite<::etcdserverpb::PutRequest>(), std::make_unique_for_overwrite<::etcdserverpb::PutResponse>(), nullptr, 0, eNodeType::EtcdNodeService, etcdserverpb::SendKVPut};
    gRpcServiceRegistry[KVDeleteRangeMessageId] = RpcService{"KV", "DeleteRange", std::make_unique_for_overwrite<::etcdserverpb::DeleteRangeRequest>(), std::make_unique_for_overwrite<::etcdserverpb::DeleteRangeResponse>(), nullptr, 0, eNodeType::EtcdNodeService, etcdserverpb::SendKVDeleteRange};
    gRpcServiceRegistry[KVTxnMessageId] = RpcService{"KV", "Txn", std::make_unique_for_overwrite<::etcdserverpb::TxnRequest>(), std::make_unique_for_overwrite<::etcdserverpb::TxnResponse>(), nullptr, 0, eNodeType::EtcdNodeService, etcdserverpb::SendKVTxn};
    gRpcServiceRegistry[KVCompactMessageId] = RpcService{"KV", "Compact", std::make_unique_for_overwrite<::etcdserverpb::CompactionRequest>(), std::make_unique_for_overwrite<::etcdserverpb::CompactionResponse>(), nullptr, 0, eNodeType::EtcdNodeService, etcdserverpb::SendKVCompact};
    gRpcServiceRegistry[LeaseLeaseGrantMessageId] = RpcService{"Lease", "LeaseGrant", std::make_unique_for_overwrite<::etcdserverpb::LeaseGrantRequest>(), std::make_unique_for_overwrite<::etcdserverpb::LeaseGrantResponse>(), nullptr, 0, eNodeType::EtcdNodeService, etcdserverpb::SendLeaseLeaseGrant};
    gRpcServiceRegistry[LeaseLeaseRevokeMessageId] = RpcService{"Lease", "LeaseRevoke", std::make_unique_for_overwrite<::etcdserverpb::LeaseRevokeRequest>(), std::make_unique_for_overwrite<::etcdserverpb::LeaseRevokeResponse>(), nullptr, 0, eNodeType::EtcdNodeService, etcdserverpb::SendLeaseLeaseRevoke};
    gRpcServiceRegistry[LeaseLeaseKeepAliveMessageId] = RpcService{"Lease", "LeaseKeepAlive", std::make_unique_for_overwrite<::etcdserverpb::LeaseKeepAliveRequest>(), std::make_unique_for_overwrite<::etcdserverpb::LeaseKeepAliveResponse>(), nullptr, 0, eNodeType::EtcdNodeService, etcdserverpb::SendLeaseLeaseKeepAlive};
    gRpcServiceRegistry[LeaseLeaseTimeToLiveMessageId] = RpcService{"Lease", "LeaseTimeToLive", std::make_unique_for_overwrite<::etcdserverpb::LeaseTimeToLiveRequest>(), std::make_unique_for_overwrite<::etcdserverpb::LeaseTimeToLiveResponse>(), nullptr, 0, eNodeType::EtcdNodeService, etcdserverpb::SendLeaseLeaseTimeToLive};
    gRpcServiceRegistry[LeaseLeaseLeasesMessageId] = RpcService{"Lease", "LeaseLeases", std::make_unique_for_overwrite<::etcdserverpb::LeaseLeasesRequest>(), std::make_unique_for_overwrite<::etcdserverpb::LeaseLeasesResponse>(), nullptr, 0, eNodeType::EtcdNodeService, etcdserverpb::SendLeaseLeaseLeases};
    gRpcServiceRegistry[LoginServiceLoginMessageId] = RpcService{"LoginService", "Login", std::make_unique_for_overwrite<::loginpb::LoginC2LRequest>(), std::make_unique_for_overwrite<::loginpb::LoginC2LResponse>(), nullptr, 1, eNodeType::LoginNodeService, loginpb::SendLoginServiceLogin};
    gRpcServiceRegistry[LoginServiceCreatePlayerMessageId] = RpcService{"LoginService", "CreatePlayer", std::make_unique_for_overwrite<::loginpb::CreatePlayerC2LRequest>(), std::make_unique_for_overwrite<::loginpb::CreatePlayerC2LResponse>(), nullptr, 1, eNodeType::LoginNodeService, loginpb::SendLoginServiceCreatePlayer};
    gRpcServiceRegistry[LoginServiceEnterGameMessageId] = RpcService{"LoginService", "EnterGame", std::make_unique_for_overwrite<::loginpb::EnterGameC2LRequest>(), std::make_unique_for_overwrite<::loginpb::EnterGameC2LResponse>(), nullptr, 1, eNodeType::LoginNodeService, loginpb::SendLoginServiceEnterGame};
    gRpcServiceRegistry[LoginServiceLeaveGameMessageId] = RpcService{"LoginService", "LeaveGame", std::make_unique_for_overwrite<::loginpb::LeaveGameC2LRequest>(), std::make_unique_for_overwrite<::Empty>(), nullptr, 1, eNodeType::LoginNodeService, loginpb::SendLoginServiceLeaveGame};
    gRpcServiceRegistry[LoginServiceDisconnectMessageId] = RpcService{"LoginService", "Disconnect", std::make_unique_for_overwrite<::loginpb::LoginNodeDisconnectRequest>(), std::make_unique_for_overwrite<::Empty>(), nullptr, 1, eNodeType::LoginNodeService, loginpb::SendLoginServiceDisconnect};
    gRpcServiceRegistry[PlayerDBServiceLoad2RedisMessageId] = RpcService{"PlayerDBService", "Load2Redis", std::make_unique_for_overwrite<::LoadPlayerRequest>(), std::make_unique_for_overwrite<::LoadPlayerResponse>(), nullptr, 0, eNodeType::DbNodeService, ::SendPlayerDBServiceLoad2Redis};
    gRpcServiceRegistry[PlayerDBServiceSave2RedisMessageId] = RpcService{"PlayerDBService", "Save2Redis", std::make_unique_for_overwrite<::SavePlayerRequest>(), std::make_unique_for_overwrite<::SavePlayerResponse>(), nullptr, 0, eNodeType::DbNodeService, ::SendPlayerDBServiceSave2Redis};
    gRpcServiceRegistry[WatchWatchMessageId] = RpcService{"Watch", "Watch", std::make_unique_for_overwrite<::etcdserverpb::WatchRequest>(), std::make_unique_for_overwrite<::etcdserverpb::WatchResponse>(), nullptr, 0, eNodeType::EtcdNodeService, etcdserverpb::SendWatchWatch};
    gClientMessageIdWhitelist.emplace(CentreClientPlayerCommonServiceSendTipToClientMessageId);
    gClientMessageIdWhitelist.emplace(CentreClientPlayerCommonServiceKickPlayerMessageId);
    gClientMessageIdWhitelist.emplace(ClientPlayerSceneServiceEnterSceneMessageId);
    gClientMessageIdWhitelist.emplace(ClientPlayerSceneServiceNotifyEnterSceneMessageId);
    gClientMessageIdWhitelist.emplace(ClientPlayerSceneServiceSceneInfoC2SMessageId);
    gClientMessageIdWhitelist.emplace(ClientPlayerSceneServiceNotifySceneInfoMessageId);
    gClientMessageIdWhitelist.emplace(ClientPlayerSceneServiceNotifyActorCreateMessageId);
    gClientMessageIdWhitelist.emplace(ClientPlayerSceneServiceNotifyActorDestroyMessageId);
    gClientMessageIdWhitelist.emplace(ClientPlayerSceneServiceNotifyActorListCreateMessageId);
    gClientMessageIdWhitelist.emplace(ClientPlayerSceneServiceNotifyActorListDestroyMessageId);
    gClientMessageIdWhitelist.emplace(ClientPlayerSkillServiceReleaseSkillMessageId);
    gClientMessageIdWhitelist.emplace(ClientPlayerSkillServiceNotifySkillUsedMessageId);
    gClientMessageIdWhitelist.emplace(ClientPlayerSkillServiceNotifySkillInterruptedMessageId);
    gClientMessageIdWhitelist.emplace(ClientPlayerSkillServiceGetSkillListMessageId);
    gClientMessageIdWhitelist.emplace(GameClientPlayerCommonServiceSendTipToClientMessageId);
    gClientMessageIdWhitelist.emplace(GameClientPlayerCommonServiceKickPlayerMessageId);
}
