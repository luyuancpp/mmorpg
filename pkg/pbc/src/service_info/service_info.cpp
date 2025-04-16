#include <array>
#include "service_info.h"
#include "proto/logic/server_player/centre_player_scene.pb.h"
#include "proto/logic/server_player/centre_player.pb.h"
#include "proto/logic/server/centre_scene.pb.h"
#include "proto/common/centre_service.pb.h"
#include "proto/logic/client_player/player_scene.pb.h"
#include "proto/logic/client_player/player_state_attribute_sync.pb.h"
#include "proto/logic/server_player/game_player_scene.pb.h"
#include "proto/logic/server_player/game_player.pb.h"
#include "proto/logic/server/game_scene.pb.h"
#include "proto/common/game_service.pb.h"
#include "proto/common/gate_service.pb.h"
#include "proto/logic/client_player/player_common.pb.h"
#include "proto/logic/client_player/player_skill.pb.h"

#include "centre_player_scene_service_info.h"
#include "centre_player_service_info.h"
#include "centre_scene_service_info.h"
#include "centre_service_service_info.h"
#include "player_scene_service_info.h"
#include "player_state_attribute_sync_service_info.h"
#include "game_player_scene_service_info.h"
#include "game_player_service_info.h"
#include "game_scene_service_info.h"
#include "game_service_service_info.h"
#include "gate_service_service_info.h"
#include "player_common_service_info.h"
#include "player_skill_service_info.h"

class CentrePlayerSceneServiceImpl final : public CentrePlayerSceneService{};
class CentrePlayerServiceImpl final : public CentrePlayerService{};
class CentreSceneServiceImpl final : public CentreSceneService{};
class CentreServiceImpl final : public CentreService{};
class ClientPlayerSceneServiceImpl final : public ClientPlayerSceneService{};
class EntitySyncServiceImpl final : public EntitySyncService{};
class GamePlayerSceneServiceImpl final : public GamePlayerSceneService{};
class GamePlayerServiceImpl final : public GamePlayerService{};
class GameSceneServiceImpl final : public GameSceneService{};
class GameServiceImpl final : public GameService{};
class GateServiceImpl final : public GateService{};
class PlayerClientCommonServiceImpl final : public PlayerClientCommonService{};
class PlayerSkillServiceImpl final : public PlayerSkillService{};


std::unordered_set<uint32_t> gClientToServerMessageId;
std::array<RpcService, 89> gMessageInfo;

void InitMessageInfo()
{
gMessageInfo[CentrePlayerSceneServiceEnterSceneMessageId] = RpcService{"CentrePlayerSceneService","EnterScene","::CentreEnterSceneRequest","::google::protobuf::Empty",std::make_unique_for_overwrite<CentrePlayerSceneServiceImpl>()};
gMessageInfo[CentrePlayerSceneServiceLeaveSceneMessageId] = RpcService{"CentrePlayerSceneService","LeaveScene","::CentreLeaveSceneRequest","::google::protobuf::Empty",std::make_unique_for_overwrite<CentrePlayerSceneServiceImpl>()};
gMessageInfo[CentrePlayerSceneServiceLeaveSceneAsyncSavePlayerCompleteMessageId] = RpcService{"CentrePlayerSceneService","LeaveSceneAsyncSavePlayerComplete","::CentreLeaveSceneAsyncSavePlayerCompleteRequest","::google::protobuf::Empty",std::make_unique_for_overwrite<CentrePlayerSceneServiceImpl>()};
gMessageInfo[CentrePlayerSceneServiceSceneInfoC2SMessageId] = RpcService{"CentrePlayerSceneService","SceneInfoC2S","::SceneInfoRequest","::google::protobuf::Empty",std::make_unique_for_overwrite<CentrePlayerSceneServiceImpl>()};

gMessageInfo[CentrePlayerServiceTestMessageId] = RpcService{"CentrePlayerService","Test","::google::protobuf::Empty","::google::protobuf::Empty",std::make_unique_for_overwrite<CentrePlayerServiceImpl>()};

gMessageInfo[CentreSceneServiceRegisterSceneMessageId] = RpcService{"CentreSceneService","RegisterScene","::RegisterSceneRequest","::RegisterSceneResponse",std::make_unique_for_overwrite<CentreSceneServiceImpl>()};
gMessageInfo[CentreSceneServiceUnRegisterSceneMessageId] = RpcService{"CentreSceneService","UnRegisterScene","::UnRegisterSceneRequest","::Empty",std::make_unique_for_overwrite<CentreSceneServiceImpl>()};

gMessageInfo[CentreServiceRegisterGameNodeMessageId] = RpcService{"CentreService","RegisterGameNode","::RegisterGameNodeRequest","::Empty",std::make_unique_for_overwrite<CentreServiceImpl>()};
gMessageInfo[CentreServiceRegisterGateNodeMessageId] = RpcService{"CentreService","RegisterGateNode","::RegisterGateNodeRequest","::Empty",std::make_unique_for_overwrite<CentreServiceImpl>()};
gMessageInfo[CentreServiceGatePlayerServiceMessageId] = RpcService{"CentreService","GatePlayerService","::GateClientMessageRequest","::Empty",std::make_unique_for_overwrite<CentreServiceImpl>()};
gMessageInfo[CentreServiceGateSessionDisconnectMessageId] = RpcService{"CentreService","GateSessionDisconnect","::GateSessionDisconnectRequest","::Empty",std::make_unique_for_overwrite<CentreServiceImpl>()};
gMessageInfo[CentreServiceLoginNodeAccountLoginMessageId] = RpcService{"CentreService","LoginNodeAccountLogin","::LoginRequest","::LoginResponse",std::make_unique_for_overwrite<CentreServiceImpl>()};
gMessageInfo[CentreServiceLoginNodeEnterGameMessageId] = RpcService{"CentreService","LoginNodeEnterGame","::CentrePlayerGameNodeEntryRequest","::Empty",std::make_unique_for_overwrite<CentreServiceImpl>()};
gMessageInfo[CentreServiceLoginNodeLeaveGameMessageId] = RpcService{"CentreService","LoginNodeLeaveGame","::LoginNodeLeaveGameRequest","::Empty",std::make_unique_for_overwrite<CentreServiceImpl>()};
gMessageInfo[CentreServiceLoginNodeSessionDisconnectMessageId] = RpcService{"CentreService","LoginNodeSessionDisconnect","::GateSessionDisconnectRequest","::Empty",std::make_unique_for_overwrite<CentreServiceImpl>()};
gMessageInfo[CentreServicePlayerServiceMessageId] = RpcService{"CentreService","PlayerService","::NodeRouteMessageRequest","::NodeRouteMessageResponse",std::make_unique_for_overwrite<CentreServiceImpl>()};
gMessageInfo[CentreServiceEnterGsSucceedMessageId] = RpcService{"CentreService","EnterGsSucceed","::EnterGameNodeSuccessRequest","::Empty",std::make_unique_for_overwrite<CentreServiceImpl>()};
gMessageInfo[CentreServiceRouteNodeStringMsgMessageId] = RpcService{"CentreService","RouteNodeStringMsg","::RouteMessageRequest","::RouteMessageResponse",std::make_unique_for_overwrite<CentreServiceImpl>()};
gMessageInfo[CentreServiceRoutePlayerStringMsgMessageId] = RpcService{"CentreService","RoutePlayerStringMsg","::RoutePlayerMessageRequest","::RoutePlayerMessageResponse",std::make_unique_for_overwrite<CentreServiceImpl>()};
gMessageInfo[CentreServiceUnRegisterGameNodeMessageId] = RpcService{"CentreService","UnRegisterGameNode","::UnregisterGameNodeRequest","::Empty",std::make_unique_for_overwrite<CentreServiceImpl>()};

gMessageInfo[ClientPlayerSceneServiceEnterSceneMessageId] = RpcService{"ClientPlayerSceneService","EnterScene","::EnterSceneC2SRequest","::EnterSceneC2SResponse",std::make_unique_for_overwrite<ClientPlayerSceneServiceImpl>()};
gClientToServerMessageId.emplace(ClientPlayerSceneServiceEnterSceneMessageId);
gMessageInfo[ClientPlayerSceneServiceNotifyEnterSceneMessageId] = RpcService{"ClientPlayerSceneService","NotifyEnterScene","::EnterSceneS2C","::Empty",std::make_unique_for_overwrite<ClientPlayerSceneServiceImpl>()};
gClientToServerMessageId.emplace(ClientPlayerSceneServiceNotifyEnterSceneMessageId);
gMessageInfo[ClientPlayerSceneServiceSceneInfoC2SMessageId] = RpcService{"ClientPlayerSceneService","SceneInfoC2S","::SceneInfoRequest","::Empty",std::make_unique_for_overwrite<ClientPlayerSceneServiceImpl>()};
gClientToServerMessageId.emplace(ClientPlayerSceneServiceSceneInfoC2SMessageId);
gMessageInfo[ClientPlayerSceneServiceNotifySceneInfoMessageId] = RpcService{"ClientPlayerSceneService","NotifySceneInfo","::SceneInfoS2C","::Empty",std::make_unique_for_overwrite<ClientPlayerSceneServiceImpl>()};
gClientToServerMessageId.emplace(ClientPlayerSceneServiceNotifySceneInfoMessageId);
gMessageInfo[ClientPlayerSceneServiceNotifyActorCreateMessageId] = RpcService{"ClientPlayerSceneService","NotifyActorCreate","::ActorCreateS2C","::Empty",std::make_unique_for_overwrite<ClientPlayerSceneServiceImpl>()};
gClientToServerMessageId.emplace(ClientPlayerSceneServiceNotifyActorCreateMessageId);
gMessageInfo[ClientPlayerSceneServiceNotifyActorDestroyMessageId] = RpcService{"ClientPlayerSceneService","NotifyActorDestroy","::ActorDestroyS2C","::Empty",std::make_unique_for_overwrite<ClientPlayerSceneServiceImpl>()};
gClientToServerMessageId.emplace(ClientPlayerSceneServiceNotifyActorDestroyMessageId);
gMessageInfo[ClientPlayerSceneServiceNotifyActorListCreateMessageId] = RpcService{"ClientPlayerSceneService","NotifyActorListCreate","::ActorListCreateS2C","::Empty",std::make_unique_for_overwrite<ClientPlayerSceneServiceImpl>()};
gClientToServerMessageId.emplace(ClientPlayerSceneServiceNotifyActorListCreateMessageId);
gMessageInfo[ClientPlayerSceneServiceNotifyActorListDestroyMessageId] = RpcService{"ClientPlayerSceneService","NotifyActorListDestroy","::ActorListDestroyS2C","::Empty",std::make_unique_for_overwrite<ClientPlayerSceneServiceImpl>()};
gClientToServerMessageId.emplace(ClientPlayerSceneServiceNotifyActorListDestroyMessageId);

gMessageInfo[EntitySyncServiceSyncBaseAttributeMessageId] = RpcService{"EntitySyncService","SyncBaseAttribute","::BaseAttributeSyncDataS2C","::Empty",std::make_unique_for_overwrite<EntitySyncServiceImpl>()};
gClientToServerMessageId.emplace(EntitySyncServiceSyncBaseAttributeMessageId);
gMessageInfo[EntitySyncServiceSyncAttribute2FramesMessageId] = RpcService{"EntitySyncService","SyncAttribute2Frames","::AttributeDelta2FramesS2C","::Empty",std::make_unique_for_overwrite<EntitySyncServiceImpl>()};
gClientToServerMessageId.emplace(EntitySyncServiceSyncAttribute2FramesMessageId);
gMessageInfo[EntitySyncServiceSyncAttribute5FramesMessageId] = RpcService{"EntitySyncService","SyncAttribute5Frames","::AttributeDelta5FramesS2C","::Empty",std::make_unique_for_overwrite<EntitySyncServiceImpl>()};
gClientToServerMessageId.emplace(EntitySyncServiceSyncAttribute5FramesMessageId);
gMessageInfo[EntitySyncServiceSyncAttribute10FramesMessageId] = RpcService{"EntitySyncService","SyncAttribute10Frames","::AttributeDelta10FramesS2C","::Empty",std::make_unique_for_overwrite<EntitySyncServiceImpl>()};
gClientToServerMessageId.emplace(EntitySyncServiceSyncAttribute10FramesMessageId);
gMessageInfo[EntitySyncServiceSyncAttribute30FramesMessageId] = RpcService{"EntitySyncService","SyncAttribute30Frames","::AttributeDelta30FramesS2C","::Empty",std::make_unique_for_overwrite<EntitySyncServiceImpl>()};
gClientToServerMessageId.emplace(EntitySyncServiceSyncAttribute30FramesMessageId);
gMessageInfo[EntitySyncServiceSyncAttribute60FramesMessageId] = RpcService{"EntitySyncService","SyncAttribute60Frames","::AttributeDelta60FramesS2C","::Empty",std::make_unique_for_overwrite<EntitySyncServiceImpl>()};
gClientToServerMessageId.emplace(EntitySyncServiceSyncAttribute60FramesMessageId);

gMessageInfo[GamePlayerSceneServiceEnterSceneMessageId] = RpcService{"GamePlayerSceneService","EnterScene","::GsEnterSceneRequest","::google::protobuf::Empty",std::make_unique_for_overwrite<GamePlayerSceneServiceImpl>()};
gMessageInfo[GamePlayerSceneServiceLeaveSceneMessageId] = RpcService{"GamePlayerSceneService","LeaveScene","::GsLeaveSceneRequest","::google::protobuf::Empty",std::make_unique_for_overwrite<GamePlayerSceneServiceImpl>()};
gMessageInfo[GamePlayerSceneServiceEnterSceneS2CMessageId] = RpcService{"GamePlayerSceneService","EnterSceneS2C","::EnterSceneS2CRequest","::EnterScenerS2CResponse",std::make_unique_for_overwrite<GamePlayerSceneServiceImpl>()};

gMessageInfo[GamePlayerServiceCentre2GsLoginMessageId] = RpcService{"GamePlayerService","Centre2GsLogin","::Centre2GsLoginRequest","::google::protobuf::Empty",std::make_unique_for_overwrite<GamePlayerServiceImpl>()};
gMessageInfo[GamePlayerServiceExitGameMessageId] = RpcService{"GamePlayerService","ExitGame","::GameNodeExitGameRequest","::google::protobuf::Empty",std::make_unique_for_overwrite<GamePlayerServiceImpl>()};

gMessageInfo[GameSceneServiceTestMessageId] = RpcService{"GameSceneService","Test","::GameSceneTest","::Empty",std::make_unique_for_overwrite<GameSceneServiceImpl>()};

gMessageInfo[GameServicePlayerEnterGameNodeMessageId] = RpcService{"GameService","PlayerEnterGameNode","::PlayerEnterGameNodeRequest","::Empty",std::make_unique_for_overwrite<GameServiceImpl>()};
gMessageInfo[GameServiceSendMessageToPlayerMessageId] = RpcService{"GameService","SendMessageToPlayer","::NodeRouteMessageRequest","::NodeRouteMessageResponse",std::make_unique_for_overwrite<GameServiceImpl>()};
gMessageInfo[GameServiceClientSendMessageToPlayerMessageId] = RpcService{"GameService","ClientSendMessageToPlayer","::ClientSendMessageToPlayerRequest","::ClientSendMessageToPlayerResponse",std::make_unique_for_overwrite<GameServiceImpl>()};
gMessageInfo[GameServiceCentreSendToPlayerViaGameNodeMessageId] = RpcService{"GameService","CentreSendToPlayerViaGameNode","::NodeRouteMessageRequest","::Empty",std::make_unique_for_overwrite<GameServiceImpl>()};
gMessageInfo[GameServiceInvokePlayerServiceMessageId] = RpcService{"GameService","InvokePlayerService","::NodeRouteMessageRequest","::NodeRouteMessageResponse",std::make_unique_for_overwrite<GameServiceImpl>()};
gMessageInfo[GameServiceRouteNodeStringMsgMessageId] = RpcService{"GameService","RouteNodeStringMsg","::RouteMessageRequest","::RouteMessageResponse",std::make_unique_for_overwrite<GameServiceImpl>()};
gMessageInfo[GameServiceRoutePlayerStringMsgMessageId] = RpcService{"GameService","RoutePlayerStringMsg","::RoutePlayerMessageRequest","::RoutePlayerMessageResponse",std::make_unique_for_overwrite<GameServiceImpl>()};
gMessageInfo[GameServiceUpdateSessionDetailMessageId] = RpcService{"GameService","UpdateSessionDetail","::RegisterPlayerSessionRequest","::Empty",std::make_unique_for_overwrite<GameServiceImpl>()};
gMessageInfo[GameServiceEnterSceneMessageId] = RpcService{"GameService","EnterScene","::Centre2GsEnterSceneRequest","::Empty",std::make_unique_for_overwrite<GameServiceImpl>()};
gMessageInfo[GameServiceCreateSceneMessageId] = RpcService{"GameService","CreateScene","::CreateSceneRequest","::CreateSceneResponse",std::make_unique_for_overwrite<GameServiceImpl>()};

gMessageInfo[GateServicePlayerEnterGameNodeMessageId] = RpcService{"GateService","PlayerEnterGameNode","::RegisterGameNodeSessionRequest","::RegisterGameNodeSessionResponse",std::make_unique_for_overwrite<GateServiceImpl>()};
gMessageInfo[GateServiceSendMessageToPlayerMessageId] = RpcService{"GateService","SendMessageToPlayer","::NodeRouteMessageRequest","::Empty",std::make_unique_for_overwrite<GateServiceImpl>()};
gMessageInfo[GateServiceKickSessionByCentreMessageId] = RpcService{"GateService","KickSessionByCentre","::KickSessionRequest","::Empty",std::make_unique_for_overwrite<GateServiceImpl>()};
gMessageInfo[GateServiceRouteNodeMessageMessageId] = RpcService{"GateService","RouteNodeMessage","::RouteMessageRequest","::RouteMessageResponse",std::make_unique_for_overwrite<GateServiceImpl>()};
gMessageInfo[GateServiceRoutePlayerMessageMessageId] = RpcService{"GateService","RoutePlayerMessage","::RoutePlayerMessageRequest","::RoutePlayerMessageResponse",std::make_unique_for_overwrite<GateServiceImpl>()};
gMessageInfo[GateServiceBroadcastToPlayersMessageId] = RpcService{"GateService","BroadcastToPlayers","::BroadcastToPlayersRequest","::Empty",std::make_unique_for_overwrite<GateServiceImpl>()};

gMessageInfo[PlayerClientCommonServiceSendTipToClientMessageId] = RpcService{"PlayerClientCommonService","SendTipToClient","::TipInfoMessage","::Empty",std::make_unique_for_overwrite<PlayerClientCommonServiceImpl>()};
gClientToServerMessageId.emplace(PlayerClientCommonServiceSendTipToClientMessageId);
gMessageInfo[PlayerClientCommonServiceKickPlayerMessageId] = RpcService{"PlayerClientCommonService","KickPlayer","::TipInfoMessage","::Empty",std::make_unique_for_overwrite<PlayerClientCommonServiceImpl>()};
gClientToServerMessageId.emplace(PlayerClientCommonServiceKickPlayerMessageId);

gMessageInfo[PlayerSkillServiceReleaseSkillMessageId] = RpcService{"PlayerSkillService","ReleaseSkill","::ReleaseSkillSkillRequest","::ReleaseSkillSkillResponse",std::make_unique_for_overwrite<PlayerSkillServiceImpl>()};
gClientToServerMessageId.emplace(PlayerSkillServiceReleaseSkillMessageId);
gMessageInfo[PlayerSkillServiceNotifySkillUsedMessageId] = RpcService{"PlayerSkillService","NotifySkillUsed","::SkillUsedS2C","::Empty",std::make_unique_for_overwrite<PlayerSkillServiceImpl>()};
gClientToServerMessageId.emplace(PlayerSkillServiceNotifySkillUsedMessageId);
gMessageInfo[PlayerSkillServiceNotifySkillInterruptedMessageId] = RpcService{"PlayerSkillService","NotifySkillInterrupted","::SkillInterruptedS2C","::Empty",std::make_unique_for_overwrite<PlayerSkillServiceImpl>()};
gClientToServerMessageId.emplace(PlayerSkillServiceNotifySkillInterruptedMessageId);
gMessageInfo[PlayerSkillServiceGetSkillListMessageId] = RpcService{"PlayerSkillService","GetSkillList","::GetSkillListRequest","::GetSkillListResponse",std::make_unique_for_overwrite<PlayerSkillServiceImpl>()};
gClientToServerMessageId.emplace(PlayerSkillServiceGetSkillListMessageId);

}
