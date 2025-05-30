#include <array>
#include "service_info.h"
#include "proto/centre/centre_client_player.pb.h"

#include "proto/centre/centre_player_scene.pb.h"

#include "proto/centre/centre_player.pb.h"

#include "proto/centre/centre_scene.pb.h"

#include "proto/centre/centre_service.pb.h"

#include "proto/game/player_scene.pb.h"

#include "proto/game/player_skill.pb.h"

#include "proto/game/player_state_attribute_sync.pb.h"

#include "proto/game/game_client_player.pb.h"

#include "proto/game/game_player_scene.pb.h"

#include "proto/game/game_player.pb.h"

#include "proto/game/game_scene.pb.h"

#include "proto/game/game_service.pb.h"

#include "proto/gate/gate_service.pb.h"

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
std::array<RpcService, 85> gMessageInfo;

void InitMessageInfo()
{
    gMessageInfo[CentreClientPlayerCommonServiceSendTipToClientMessageId] = RpcService{"CentreClientPlayerCommonService", "SendTipToClient", "::TipInfoMessage", "::Empty", std::make_unique_for_overwrite<CentreClientPlayerCommonServiceImpl>(), 0};
    gMessageInfo[CentreClientPlayerCommonServiceKickPlayerMessageId] = RpcService{"CentreClientPlayerCommonService", "KickPlayer", "::CentreKickPlayerRequest", "::Empty", std::make_unique_for_overwrite<CentreClientPlayerCommonServiceImpl>(), 0};
    gMessageInfo[CentrePlayerSceneServiceEnterSceneMessageId] = RpcService{"CentrePlayerSceneService", "EnterScene", "::CentreEnterSceneRequest", "::google::protobuf::Empty", std::make_unique_for_overwrite<CentrePlayerSceneServiceImpl>(), 0};
    gMessageInfo[CentrePlayerSceneServiceLeaveSceneMessageId] = RpcService{"CentrePlayerSceneService", "LeaveScene", "::CentreLeaveSceneRequest", "::google::protobuf::Empty", std::make_unique_for_overwrite<CentrePlayerSceneServiceImpl>(), 0};
    gMessageInfo[CentrePlayerSceneServiceLeaveSceneAsyncSavePlayerCompleteMessageId] = RpcService{"CentrePlayerSceneService", "LeaveSceneAsyncSavePlayerComplete", "::CentreLeaveSceneAsyncSavePlayerCompleteRequest", "::google::protobuf::Empty", std::make_unique_for_overwrite<CentrePlayerSceneServiceImpl>(), 0};
    gMessageInfo[CentrePlayerSceneServiceSceneInfoC2SMessageId] = RpcService{"CentrePlayerSceneService", "SceneInfoC2S", "::CentreSceneInfoRequest", "::google::protobuf::Empty", std::make_unique_for_overwrite<CentrePlayerSceneServiceImpl>(), 0};
    gMessageInfo[CentrePlayerServiceTestMessageId] = RpcService{"CentrePlayerService", "Test", "::google::protobuf::Empty", "::google::protobuf::Empty", std::make_unique_for_overwrite<CentrePlayerServiceImpl>(), 0};
    gMessageInfo[CentreSceneServiceRegisterSceneMessageId] = RpcService{"CentreSceneService", "RegisterScene", "::RegisterSceneRequest", "::RegisterSceneResponse", std::make_unique_for_overwrite<CentreSceneServiceImpl>(), 0};
    gMessageInfo[CentreSceneServiceUnRegisterSceneMessageId] = RpcService{"CentreSceneService", "UnRegisterScene", "::UnRegisterSceneRequest", "::Empty", std::make_unique_for_overwrite<CentreSceneServiceImpl>(), 0};
    gMessageInfo[CentreServiceGatePlayerServiceMessageId] = RpcService{"CentreService", "GatePlayerService", "::GateClientMessageRequest", "::Empty", std::make_unique_for_overwrite<CentreServiceImpl>(), 0};
    gMessageInfo[CentreServiceGateSessionDisconnectMessageId] = RpcService{"CentreService", "GateSessionDisconnect", "::GateSessionDisconnectRequest", "::Empty", std::make_unique_for_overwrite<CentreServiceImpl>(), 0};
    gMessageInfo[CentreServiceLoginNodeAccountLoginMessageId] = RpcService{"CentreService", "LoginNodeAccountLogin", "::LoginRequest", "::LoginResponse", std::make_unique_for_overwrite<CentreServiceImpl>(), 0};
    gMessageInfo[CentreServiceLoginNodeEnterGameMessageId] = RpcService{"CentreService", "LoginNodeEnterGame", "::CentrePlayerGameNodeEntryRequest", "::Empty", std::make_unique_for_overwrite<CentreServiceImpl>(), 0};
    gMessageInfo[CentreServiceLoginNodeLeaveGameMessageId] = RpcService{"CentreService", "LoginNodeLeaveGame", "::LoginNodeLeaveGameRequest", "::Empty", std::make_unique_for_overwrite<CentreServiceImpl>(), 0};
    gMessageInfo[CentreServiceLoginNodeSessionDisconnectMessageId] = RpcService{"CentreService", "LoginNodeSessionDisconnect", "::GateSessionDisconnectRequest", "::Empty", std::make_unique_for_overwrite<CentreServiceImpl>(), 0};
    gMessageInfo[CentreServicePlayerServiceMessageId] = RpcService{"CentreService", "PlayerService", "::NodeRouteMessageRequest", "::NodeRouteMessageResponse", std::make_unique_for_overwrite<CentreServiceImpl>(), 0};
    gMessageInfo[CentreServiceEnterGsSucceedMessageId] = RpcService{"CentreService", "EnterGsSucceed", "::EnterGameNodeSuccessRequest", "::Empty", std::make_unique_for_overwrite<CentreServiceImpl>(), 0};
    gMessageInfo[CentreServiceRouteNodeStringMsgMessageId] = RpcService{"CentreService", "RouteNodeStringMsg", "::RouteMessageRequest", "::RouteMessageResponse", std::make_unique_for_overwrite<CentreServiceImpl>(), 0};
    gMessageInfo[CentreServiceRoutePlayerStringMsgMessageId] = RpcService{"CentreService", "RoutePlayerStringMsg", "::RoutePlayerMessageRequest", "::RoutePlayerMessageResponse", std::make_unique_for_overwrite<CentreServiceImpl>(), 0};
    gMessageInfo[CentreServiceInitSceneNodeMessageId] = RpcService{"CentreService", "InitSceneNode", "::InitSceneNodeRequest", "::Empty", std::make_unique_for_overwrite<CentreServiceImpl>(), 0};
    gMessageInfo[CentreServiceRegisterNodeSessionMessageId] = RpcService{"CentreService", "RegisterNodeSession", "::RegisterNodeSessionRequest", "::RegisterNodeSessionResponse", std::make_unique_for_overwrite<CentreServiceImpl>(), 0};
    gMessageInfo[ClientPlayerSceneServiceEnterSceneMessageId] = RpcService{"ClientPlayerSceneService", "EnterScene", "::EnterSceneC2SRequest", "::EnterSceneC2SResponse", std::make_unique_for_overwrite<ClientPlayerSceneServiceImpl>(), 0};
    gMessageInfo[ClientPlayerSceneServiceNotifyEnterSceneMessageId] = RpcService{"ClientPlayerSceneService", "NotifyEnterScene", "::EnterSceneS2C", "::Empty", std::make_unique_for_overwrite<ClientPlayerSceneServiceImpl>(), 0};
    gMessageInfo[ClientPlayerSceneServiceSceneInfoC2SMessageId] = RpcService{"ClientPlayerSceneService", "SceneInfoC2S", "::SceneInfoRequest", "::Empty", std::make_unique_for_overwrite<ClientPlayerSceneServiceImpl>(), 0};
    gMessageInfo[ClientPlayerSceneServiceNotifySceneInfoMessageId] = RpcService{"ClientPlayerSceneService", "NotifySceneInfo", "::SceneInfoS2C", "::Empty", std::make_unique_for_overwrite<ClientPlayerSceneServiceImpl>(), 0};
    gMessageInfo[ClientPlayerSceneServiceNotifyActorCreateMessageId] = RpcService{"ClientPlayerSceneService", "NotifyActorCreate", "::ActorCreateS2C", "::Empty", std::make_unique_for_overwrite<ClientPlayerSceneServiceImpl>(), 0};
    gMessageInfo[ClientPlayerSceneServiceNotifyActorDestroyMessageId] = RpcService{"ClientPlayerSceneService", "NotifyActorDestroy", "::ActorDestroyS2C", "::Empty", std::make_unique_for_overwrite<ClientPlayerSceneServiceImpl>(), 0};
    gMessageInfo[ClientPlayerSceneServiceNotifyActorListCreateMessageId] = RpcService{"ClientPlayerSceneService", "NotifyActorListCreate", "::ActorListCreateS2C", "::Empty", std::make_unique_for_overwrite<ClientPlayerSceneServiceImpl>(), 0};
    gMessageInfo[ClientPlayerSceneServiceNotifyActorListDestroyMessageId] = RpcService{"ClientPlayerSceneService", "NotifyActorListDestroy", "::ActorListDestroyS2C", "::Empty", std::make_unique_for_overwrite<ClientPlayerSceneServiceImpl>(), 0};
    gMessageInfo[ClientPlayerSkillServiceReleaseSkillMessageId] = RpcService{"ClientPlayerSkillService", "ReleaseSkill", "::ReleaseSkillSkillRequest", "::ReleaseSkillSkillResponse", std::make_unique_for_overwrite<ClientPlayerSkillServiceImpl>(), 0};
    gMessageInfo[ClientPlayerSkillServiceNotifySkillUsedMessageId] = RpcService{"ClientPlayerSkillService", "NotifySkillUsed", "::SkillUsedS2C", "::Empty", std::make_unique_for_overwrite<ClientPlayerSkillServiceImpl>(), 0};
    gMessageInfo[ClientPlayerSkillServiceNotifySkillInterruptedMessageId] = RpcService{"ClientPlayerSkillService", "NotifySkillInterrupted", "::SkillInterruptedS2C", "::Empty", std::make_unique_for_overwrite<ClientPlayerSkillServiceImpl>(), 0};
    gMessageInfo[ClientPlayerSkillServiceGetSkillListMessageId] = RpcService{"ClientPlayerSkillService", "GetSkillList", "::GetSkillListRequest", "::GetSkillListResponse", std::make_unique_for_overwrite<ClientPlayerSkillServiceImpl>(), 0};
    gMessageInfo[EntitySyncServiceSyncBaseAttributeMessageId] = RpcService{"EntitySyncService", "SyncBaseAttribute", "::BaseAttributeSyncDataS2C", "::Empty", std::make_unique_for_overwrite<EntitySyncServiceImpl>(), 0};
    gMessageInfo[EntitySyncServiceSyncAttribute2FramesMessageId] = RpcService{"EntitySyncService", "SyncAttribute2Frames", "::AttributeDelta2FramesS2C", "::Empty", std::make_unique_for_overwrite<EntitySyncServiceImpl>(), 0};
    gMessageInfo[EntitySyncServiceSyncAttribute5FramesMessageId] = RpcService{"EntitySyncService", "SyncAttribute5Frames", "::AttributeDelta5FramesS2C", "::Empty", std::make_unique_for_overwrite<EntitySyncServiceImpl>(), 0};
    gMessageInfo[EntitySyncServiceSyncAttribute10FramesMessageId] = RpcService{"EntitySyncService", "SyncAttribute10Frames", "::AttributeDelta10FramesS2C", "::Empty", std::make_unique_for_overwrite<EntitySyncServiceImpl>(), 0};
    gMessageInfo[EntitySyncServiceSyncAttribute30FramesMessageId] = RpcService{"EntitySyncService", "SyncAttribute30Frames", "::AttributeDelta30FramesS2C", "::Empty", std::make_unique_for_overwrite<EntitySyncServiceImpl>(), 0};
    gMessageInfo[EntitySyncServiceSyncAttribute60FramesMessageId] = RpcService{"EntitySyncService", "SyncAttribute60Frames", "::AttributeDelta60FramesS2C", "::Empty", std::make_unique_for_overwrite<EntitySyncServiceImpl>(), 0};
    gMessageInfo[GameClientPlayerCommonServiceSendTipToClientMessageId] = RpcService{"GameClientPlayerCommonService", "SendTipToClient", "::TipInfoMessage", "::Empty", std::make_unique_for_overwrite<GameClientPlayerCommonServiceImpl>(), 0};
    gMessageInfo[GameClientPlayerCommonServiceKickPlayerMessageId] = RpcService{"GameClientPlayerCommonService", "KickPlayer", "::GameKickPlayerRequest", "::Empty", std::make_unique_for_overwrite<GameClientPlayerCommonServiceImpl>(), 0};
    gMessageInfo[GamePlayerSceneServiceEnterSceneMessageId] = RpcService{"GamePlayerSceneService", "EnterScene", "::GsEnterSceneRequest", "::google::protobuf::Empty", std::make_unique_for_overwrite<GamePlayerSceneServiceImpl>(), 0};
    gMessageInfo[GamePlayerSceneServiceLeaveSceneMessageId] = RpcService{"GamePlayerSceneService", "LeaveScene", "::GsLeaveSceneRequest", "::google::protobuf::Empty", std::make_unique_for_overwrite<GamePlayerSceneServiceImpl>(), 0};
    gMessageInfo[GamePlayerSceneServiceEnterSceneS2CMessageId] = RpcService{"GamePlayerSceneService", "EnterSceneS2C", "::EnterSceneS2CRequest", "::EnterScenerS2CResponse", std::make_unique_for_overwrite<GamePlayerSceneServiceImpl>(), 0};
    gMessageInfo[GamePlayerServiceCentre2GsLoginMessageId] = RpcService{"GamePlayerService", "Centre2GsLogin", "::Centre2GsLoginRequest", "::google::protobuf::Empty", std::make_unique_for_overwrite<GamePlayerServiceImpl>(), 0};
    gMessageInfo[GamePlayerServiceExitGameMessageId] = RpcService{"GamePlayerService", "ExitGame", "::GameNodeExitGameRequest", "::google::protobuf::Empty", std::make_unique_for_overwrite<GamePlayerServiceImpl>(), 0};
    gMessageInfo[GameSceneServiceTestMessageId] = RpcService{"GameSceneService", "Test", "::GameSceneTest", "::Empty", std::make_unique_for_overwrite<GameSceneServiceImpl>(), 0};
    gMessageInfo[GameServicePlayerEnterGameNodeMessageId] = RpcService{"GameService", "PlayerEnterGameNode", "::PlayerEnterGameNodeRequest", "::Empty", std::make_unique_for_overwrite<GameServiceImpl>(), 0};
    gMessageInfo[GameServiceSendMessageToPlayerMessageId] = RpcService{"GameService", "SendMessageToPlayer", "::NodeRouteMessageRequest", "::NodeRouteMessageResponse", std::make_unique_for_overwrite<GameServiceImpl>(), 0};
    gMessageInfo[GameServiceClientSendMessageToPlayerMessageId] = RpcService{"GameService", "ClientSendMessageToPlayer", "::ClientSendMessageToPlayerRequest", "::ClientSendMessageToPlayerResponse", std::make_unique_for_overwrite<GameServiceImpl>(), 0};
    gMessageInfo[GameServiceCentreSendToPlayerViaGameNodeMessageId] = RpcService{"GameService", "CentreSendToPlayerViaGameNode", "::NodeRouteMessageRequest", "::Empty", std::make_unique_for_overwrite<GameServiceImpl>(), 0};
    gMessageInfo[GameServiceInvokePlayerServiceMessageId] = RpcService{"GameService", "InvokePlayerService", "::NodeRouteMessageRequest", "::NodeRouteMessageResponse", std::make_unique_for_overwrite<GameServiceImpl>(), 0};
    gMessageInfo[GameServiceRouteNodeStringMsgMessageId] = RpcService{"GameService", "RouteNodeStringMsg", "::RouteMessageRequest", "::RouteMessageResponse", std::make_unique_for_overwrite<GameServiceImpl>(), 0};
    gMessageInfo[GameServiceRoutePlayerStringMsgMessageId] = RpcService{"GameService", "RoutePlayerStringMsg", "::RoutePlayerMessageRequest", "::RoutePlayerMessageResponse", std::make_unique_for_overwrite<GameServiceImpl>(), 0};
    gMessageInfo[GameServiceUpdateSessionDetailMessageId] = RpcService{"GameService", "UpdateSessionDetail", "::RegisterPlayerSessionRequest", "::Empty", std::make_unique_for_overwrite<GameServiceImpl>(), 0};
    gMessageInfo[GameServiceEnterSceneMessageId] = RpcService{"GameService", "EnterScene", "::Centre2GsEnterSceneRequest", "::Empty", std::make_unique_for_overwrite<GameServiceImpl>(), 0};
    gMessageInfo[GameServiceCreateSceneMessageId] = RpcService{"GameService", "CreateScene", "::CreateSceneRequest", "::CreateSceneResponse", std::make_unique_for_overwrite<GameServiceImpl>(), 0};
    gMessageInfo[GameServiceRegisterNodeSessionMessageId] = RpcService{"GameService", "RegisterNodeSession", "::RegisterNodeSessionRequest", "::RegisterNodeSessionResponse", std::make_unique_for_overwrite<GameServiceImpl>(), 0};
    gMessageInfo[GateServicePlayerEnterGameNodeMessageId] = RpcService{"GateService", "PlayerEnterGameNode", "::RegisterGameNodeSessionRequest", "::RegisterGameNodeSessionResponse", std::make_unique_for_overwrite<GateServiceImpl>(), 0};
    gMessageInfo[GateServiceSendMessageToPlayerMessageId] = RpcService{"GateService", "SendMessageToPlayer", "::NodeRouteMessageRequest", "::Empty", std::make_unique_for_overwrite<GateServiceImpl>(), 0};
    gMessageInfo[GateServiceKickSessionByCentreMessageId] = RpcService{"GateService", "KickSessionByCentre", "::KickSessionRequest", "::Empty", std::make_unique_for_overwrite<GateServiceImpl>(), 0};
    gMessageInfo[GateServiceRouteNodeMessageMessageId] = RpcService{"GateService", "RouteNodeMessage", "::RouteMessageRequest", "::RouteMessageResponse", std::make_unique_for_overwrite<GateServiceImpl>(), 0};
    gMessageInfo[GateServiceRoutePlayerMessageMessageId] = RpcService{"GateService", "RoutePlayerMessage", "::RoutePlayerMessageRequest", "::RoutePlayerMessageResponse", std::make_unique_for_overwrite<GateServiceImpl>(), 0};
    gMessageInfo[GateServiceBroadcastToPlayersMessageId] = RpcService{"GateService", "BroadcastToPlayers", "::BroadcastToPlayersRequest", "::Empty", std::make_unique_for_overwrite<GateServiceImpl>(), 0};
    gMessageInfo[GateServiceRegisterNodeSessionMessageId] = RpcService{"GateService", "RegisterNodeSession", "::RegisterNodeSessionRequest", "::RegisterNodeSessionResponse", std::make_unique_for_overwrite<GateServiceImpl>(), 0};
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
