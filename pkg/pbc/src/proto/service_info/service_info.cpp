#include <array>
#include "service_info.h"
#include "logic/server_player/centre_player_scene.pb.h"
#include "logic/server_player/centre_player.pb.h"
#include "logic/server/centre_scene.pb.h"
#include "common/centre_service.pb.h"
#include "logic/client_player/player_scene.pb.h"
#include "logic/server_player/game_player_scene.pb.h"
#include "logic/server_player/game_player.pb.h"
#include "logic/server/game_scene.pb.h"
#include "common/game_service.pb.h"
#include "common/gate_service.pb.h"
#include "logic/client_player/player_common.pb.h"
#include "logic/client_player/player_skill.pb.h"

#include "centre_player_scene_service_info.h"
#include "centre_player_service_info.h"
#include "centre_scene_service_info.h"
#include "centre_service_service_info.h"
#include "player_scene_service_info.h"
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
class GamePlayerSceneServiceImpl final : public GamePlayerSceneService{};
class GamePlayerServiceImpl final : public GamePlayerService{};
class GameSceneServiceImpl final : public GameSceneService{};
class GameServiceImpl final : public GameService{};
class GateServiceImpl final : public GateService{};
class PlayerClientCommonServiceImpl final : public PlayerClientCommonService{};
class PlayerSkillServiceImpl final : public PlayerSkillService{};


std::unordered_set<uint32_t> g_c2s_service_id;
std::array<RpcService, 76> g_message_info;

void InitMessageInfo()
{
g_message_info[CentrePlayerSceneServiceEnterSceneMessageId] = RpcService{"CentrePlayerSceneService","EnterScene","CentreEnterSceneRequest","google::protobuf::Empty",std::make_unique_for_overwrite<CentrePlayerSceneServiceImpl>()};
g_message_info[CentrePlayerSceneServiceLeaveSceneMessageId] = RpcService{"CentrePlayerSceneService","LeaveScene","CentreLeaveSceneRequest","google::protobuf::Empty",std::make_unique_for_overwrite<CentrePlayerSceneServiceImpl>()};
g_message_info[CentrePlayerSceneServiceLeaveSceneAsyncSavePlayerCompleteMessageId] = RpcService{"CentrePlayerSceneService","LeaveSceneAsyncSavePlayerComplete","CentreLeaveSceneAsyncSavePlayerCompleteRequest","google::protobuf::Empty",std::make_unique_for_overwrite<CentrePlayerSceneServiceImpl>()};
g_message_info[CentrePlayerSceneServiceSceneInfoC2SMessageId] = RpcService{"CentrePlayerSceneService","SceneInfoC2S","SceneInfoRequest","google::protobuf::Empty",std::make_unique_for_overwrite<CentrePlayerSceneServiceImpl>()};

g_message_info[CentrePlayerServiceTestMessageId] = RpcService{"CentrePlayerService","Test","google::protobuf::Empty","google::protobuf::Empty",std::make_unique_for_overwrite<CentrePlayerServiceImpl>()};

g_message_info[CentreSceneServiceRegisterSceneMessageId] = RpcService{"CentreSceneService","RegisterScene","RegisterSceneRequest","RegisterSceneResponse",std::make_unique_for_overwrite<CentreSceneServiceImpl>()};
g_message_info[CentreSceneServiceUnRegisterSceneMessageId] = RpcService{"CentreSceneService","UnRegisterScene","UnRegisterSceneRequest","Empty",std::make_unique_for_overwrite<CentreSceneServiceImpl>()};

g_message_info[CentreServiceRegisterGameNodeMessageId] = RpcService{"CentreService","RegisterGameNode","RegisterGameNodeRequest","Empty",std::make_unique_for_overwrite<CentreServiceImpl>()};
g_message_info[CentreServiceRegisterGateNodeMessageId] = RpcService{"CentreService","RegisterGateNode","RegisterGateNodeRequest","Empty",std::make_unique_for_overwrite<CentreServiceImpl>()};
g_message_info[CentreServiceGatePlayerServiceMessageId] = RpcService{"CentreService","GatePlayerService","GateClientMessageRequest","Empty",std::make_unique_for_overwrite<CentreServiceImpl>()};
g_message_info[CentreServiceGateSessionDisconnectMessageId] = RpcService{"CentreService","GateSessionDisconnect","GateSessionDisconnectRequest","Empty",std::make_unique_for_overwrite<CentreServiceImpl>()};
g_message_info[CentreServiceLoginNodeAccountLoginMessageId] = RpcService{"CentreService","LoginNodeAccountLogin","LoginRequest","LoginResponse",std::make_unique_for_overwrite<CentreServiceImpl>()};
g_message_info[CentreServiceLoginNodeEnterGameMessageId] = RpcService{"CentreService","LoginNodeEnterGame","CentrePlayerGameNodeEntryRequest","Empty",std::make_unique_for_overwrite<CentreServiceImpl>()};
g_message_info[CentreServiceLoginNodeLeaveGameMessageId] = RpcService{"CentreService","LoginNodeLeaveGame","LoginNodeLeaveGameRequest","Empty",std::make_unique_for_overwrite<CentreServiceImpl>()};
g_message_info[CentreServiceLoginNodeSessionDisconnectMessageId] = RpcService{"CentreService","LoginNodeSessionDisconnect","GateSessionDisconnectRequest","Empty",std::make_unique_for_overwrite<CentreServiceImpl>()};
g_message_info[CentreServicePlayerServiceMessageId] = RpcService{"CentreService","PlayerService","NodeRouteMessageRequest","NodeRouteMessageResponse",std::make_unique_for_overwrite<CentreServiceImpl>()};
g_message_info[CentreServiceEnterGsSucceedMessageId] = RpcService{"CentreService","EnterGsSucceed","EnterGameNodeSuccessRequest","Empty",std::make_unique_for_overwrite<CentreServiceImpl>()};
g_message_info[CentreServiceRouteNodeStringMsgMessageId] = RpcService{"CentreService","RouteNodeStringMsg","RouteMsgStringRequest","RouteMsgStringResponse",std::make_unique_for_overwrite<CentreServiceImpl>()};
g_message_info[CentreServiceRoutePlayerStringMsgMessageId] = RpcService{"CentreService","RoutePlayerStringMsg","RoutePlayerMsgStringRequest","RoutePlayerMsgStringResponse",std::make_unique_for_overwrite<CentreServiceImpl>()};
g_message_info[CentreServiceUnRegisterGameNodeMessageId] = RpcService{"CentreService","UnRegisterGameNode","UnRegisterGameNodeRequest","Empty",std::make_unique_for_overwrite<CentreServiceImpl>()};

g_message_info[ClientPlayerSceneServiceEnterSceneMessageId] = RpcService{"ClientPlayerSceneService","EnterScene","EnterSceneC2SRequest","EnterSceneC2SResponse",std::make_unique_for_overwrite<ClientPlayerSceneServiceImpl>()};
g_c2s_service_id.emplace(ClientPlayerSceneServiceEnterSceneMessageId);
g_message_info[ClientPlayerSceneServiceNotifyEnterSceneMessageId] = RpcService{"ClientPlayerSceneService","NotifyEnterScene","EnterSceneS2C","Empty",std::make_unique_for_overwrite<ClientPlayerSceneServiceImpl>()};
g_c2s_service_id.emplace(ClientPlayerSceneServiceNotifyEnterSceneMessageId);
g_message_info[ClientPlayerSceneServiceSceneInfoC2SMessageId] = RpcService{"ClientPlayerSceneService","SceneInfoC2S","SceneInfoRequest","Empty",std::make_unique_for_overwrite<ClientPlayerSceneServiceImpl>()};
g_c2s_service_id.emplace(ClientPlayerSceneServiceSceneInfoC2SMessageId);
g_message_info[ClientPlayerSceneServiceNotifySceneInfoMessageId] = RpcService{"ClientPlayerSceneService","NotifySceneInfo","SceneInfoS2C","Empty",std::make_unique_for_overwrite<ClientPlayerSceneServiceImpl>()};
g_c2s_service_id.emplace(ClientPlayerSceneServiceNotifySceneInfoMessageId);
g_message_info[ClientPlayerSceneServiceNotifyActorCreateMessageId] = RpcService{"ClientPlayerSceneService","NotifyActorCreate","ActorCreateS2C","Empty",std::make_unique_for_overwrite<ClientPlayerSceneServiceImpl>()};
g_c2s_service_id.emplace(ClientPlayerSceneServiceNotifyActorCreateMessageId);
g_message_info[ClientPlayerSceneServiceNotifyActorDestroyMessageId] = RpcService{"ClientPlayerSceneService","NotifyActorDestroy","ActorDestroyS2C","Empty",std::make_unique_for_overwrite<ClientPlayerSceneServiceImpl>()};
g_c2s_service_id.emplace(ClientPlayerSceneServiceNotifyActorDestroyMessageId);
g_message_info[ClientPlayerSceneServiceNotifyActorListCreateMessageId] = RpcService{"ClientPlayerSceneService","NotifyActorListCreate","ActorListCreateS2C","Empty",std::make_unique_for_overwrite<ClientPlayerSceneServiceImpl>()};
g_c2s_service_id.emplace(ClientPlayerSceneServiceNotifyActorListCreateMessageId);
g_message_info[ClientPlayerSceneServiceNotifyActorListDestroyMessageId] = RpcService{"ClientPlayerSceneService","NotifyActorListDestroy","ActorListDestroyS2C","Empty",std::make_unique_for_overwrite<ClientPlayerSceneServiceImpl>()};
g_c2s_service_id.emplace(ClientPlayerSceneServiceNotifyActorListDestroyMessageId);

g_message_info[GamePlayerSceneServiceEnterSceneMessageId] = RpcService{"GamePlayerSceneService","EnterScene","GsEnterSceneRequest","google::protobuf::Empty",std::make_unique_for_overwrite<GamePlayerSceneServiceImpl>()};
g_message_info[GamePlayerSceneServiceLeaveSceneMessageId] = RpcService{"GamePlayerSceneService","LeaveScene","GsLeaveSceneRequest","google::protobuf::Empty",std::make_unique_for_overwrite<GamePlayerSceneServiceImpl>()};
g_message_info[GamePlayerSceneServiceEnterSceneS2CMessageId] = RpcService{"GamePlayerSceneService","EnterSceneS2C","EnterSceneS2CRequest","EnterScenerS2CResponse",std::make_unique_for_overwrite<GamePlayerSceneServiceImpl>()};

g_message_info[GamePlayerServiceCentre2GsLoginMessageId] = RpcService{"GamePlayerService","Centre2GsLogin","Centre2GsLoginRequest","google::protobuf::Empty",std::make_unique_for_overwrite<GamePlayerServiceImpl>()};
g_message_info[GamePlayerServiceExitGameMessageId] = RpcService{"GamePlayerService","ExitGame","GameNodeExitGameRequest","google::protobuf::Empty",std::make_unique_for_overwrite<GamePlayerServiceImpl>()};

g_message_info[GameSceneServiceTestMessageId] = RpcService{"GameSceneService","Test","GameSceneTest","Empty",std::make_unique_for_overwrite<GameSceneServiceImpl>()};

g_message_info[GameServicePlayerEnterGameNodeMessageId] = RpcService{"GameService","PlayerEnterGameNode","PlayerEnterGameNodeRequest","Empty",std::make_unique_for_overwrite<GameServiceImpl>()};
g_message_info[GameServiceSendMessageToPlayerMessageId] = RpcService{"GameService","SendMessageToPlayer","NodeRouteMessageRequest","NodeRouteMessageResponse",std::make_unique_for_overwrite<GameServiceImpl>()};
g_message_info[GameServiceClientSendMessageToPlayerMessageId] = RpcService{"GameService","ClientSendMessageToPlayer","ClientSendMessageToPlayerRequest","ClientSendMessageToPlayerResponse",std::make_unique_for_overwrite<GameServiceImpl>()};
g_message_info[GameServiceRegisterGateNodeMessageId] = RpcService{"GameService","RegisterGateNode","RegisterGateNodeRequest","Empty",std::make_unique_for_overwrite<GameServiceImpl>()};
g_message_info[GameServiceCentreSendToPlayerViaGameNodeMessageId] = RpcService{"GameService","CentreSendToPlayerViaGameNode","NodeRouteMessageRequest","Empty",std::make_unique_for_overwrite<GameServiceImpl>()};
g_message_info[GameServiceInvokePlayerServiceMessageId] = RpcService{"GameService","InvokePlayerService","NodeRouteMessageRequest","NodeRouteMessageResponse",std::make_unique_for_overwrite<GameServiceImpl>()};
g_message_info[GameServiceRouteNodeStringMsgMessageId] = RpcService{"GameService","RouteNodeStringMsg","RouteMsgStringRequest","RouteMsgStringResponse",std::make_unique_for_overwrite<GameServiceImpl>()};
g_message_info[GameServiceRoutePlayerStringMsgMessageId] = RpcService{"GameService","RoutePlayerStringMsg","RoutePlayerMsgStringRequest","RoutePlayerMsgStringResponse",std::make_unique_for_overwrite<GameServiceImpl>()};
g_message_info[GameServiceUpdateSessionDetailMessageId] = RpcService{"GameService","UpdateSessionDetail","RegisterPlayerSessionRequest","Empty",std::make_unique_for_overwrite<GameServiceImpl>()};
g_message_info[GameServiceEnterSceneMessageId] = RpcService{"GameService","EnterScene","Centre2GsEnterSceneRequest","Empty",std::make_unique_for_overwrite<GameServiceImpl>()};
g_message_info[GameServiceCreateSceneMessageId] = RpcService{"GameService","CreateScene","CreateSceneRequest","CreateSceneResponse",std::make_unique_for_overwrite<GameServiceImpl>()};

g_message_info[GateServiceRegisterGameMessageId] = RpcService{"GateService","RegisterGame","RegisterGameNodeRequest","Empty",std::make_unique_for_overwrite<GateServiceImpl>()};
g_message_info[GateServiceUnRegisterGameMessageId] = RpcService{"GateService","UnRegisterGame","UnRegisterGameNodeRequest","Empty",std::make_unique_for_overwrite<GateServiceImpl>()};
g_message_info[GateServicePlayerEnterGameNodeMessageId] = RpcService{"GateService","PlayerEnterGameNode","RegisterGameNodeSessionRequest","RegisterGameNodeSessionResponse",std::make_unique_for_overwrite<GateServiceImpl>()};
g_message_info[GateServiceSendMessageToPlayerMessageId] = RpcService{"GateService","SendMessageToPlayer","NodeRouteMessageRequest","Empty",std::make_unique_for_overwrite<GateServiceImpl>()};
g_message_info[GateServiceKickSessionByCentreMessageId] = RpcService{"GateService","KickSessionByCentre","KickSessionRequest","Empty",std::make_unique_for_overwrite<GateServiceImpl>()};
g_message_info[GateServiceRouteNodeMessageMessageId] = RpcService{"GateService","RouteNodeMessage","RouteMsgStringRequest","RouteMsgStringResponse",std::make_unique_for_overwrite<GateServiceImpl>()};
g_message_info[GateServiceRoutePlayerMessageMessageId] = RpcService{"GateService","RoutePlayerMessage","RoutePlayerMsgStringRequest","RoutePlayerMsgStringResponse",std::make_unique_for_overwrite<GateServiceImpl>()};
g_message_info[GateServiceBroadcastToPlayersMessageId] = RpcService{"GateService","BroadcastToPlayers","BroadcastToPlayersRequest","Empty",std::make_unique_for_overwrite<GateServiceImpl>()};

g_message_info[PlayerClientCommonServiceSendTipToClientMessageId] = RpcService{"PlayerClientCommonService","SendTipToClient","TipInfoMessage","Empty",std::make_unique_for_overwrite<PlayerClientCommonServiceImpl>()};
g_c2s_service_id.emplace(PlayerClientCommonServiceSendTipToClientMessageId);
g_message_info[PlayerClientCommonServiceKickPlayerMessageId] = RpcService{"PlayerClientCommonService","KickPlayer","TipInfoMessage","Empty",std::make_unique_for_overwrite<PlayerClientCommonServiceImpl>()};
g_c2s_service_id.emplace(PlayerClientCommonServiceKickPlayerMessageId);

g_message_info[PlayerSkillServiceUseSkillMessageId] = RpcService{"PlayerSkillService","UseSkill","UseSkillRequest","UseSkillResponse",std::make_unique_for_overwrite<PlayerSkillServiceImpl>()};
g_c2s_service_id.emplace(PlayerSkillServiceUseSkillMessageId);
g_message_info[PlayerSkillServiceNotifySkillUsedMessageId] = RpcService{"PlayerSkillService","NotifySkillUsed","SkillUsedS2C","Empty",std::make_unique_for_overwrite<PlayerSkillServiceImpl>()};
g_c2s_service_id.emplace(PlayerSkillServiceNotifySkillUsedMessageId);
g_message_info[PlayerSkillServiceNotifySkillInterruptedMessageId] = RpcService{"PlayerSkillService","NotifySkillInterrupted","SkillInterruptedS2C","Empty",std::make_unique_for_overwrite<PlayerSkillServiceImpl>()};
g_c2s_service_id.emplace(PlayerSkillServiceNotifySkillInterruptedMessageId);

}
