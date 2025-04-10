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
gMessageInfo[CentrePlayerSceneServiceEnterSceneMessageId] = RpcService{"CentrePlayerSceneService","%!s(func() string=0x11e0fe0)","%!s(func() string=0x11e1060)","%!s(func() string=0x11e10e0)",std::make_unique_for_overwrite<CentrePlayerSceneServiceImpl>()};
gMessageInfo[CentrePlayerSceneServiceLeaveSceneMessageId] = RpcService{"CentrePlayerSceneService","%!s(func() string=0x11e0fe0)","%!s(func() string=0x11e1060)","%!s(func() string=0x11e10e0)",std::make_unique_for_overwrite<CentrePlayerSceneServiceImpl>()};
gMessageInfo[CentrePlayerSceneServiceLeaveSceneAsyncSavePlayerCompleteMessageId] = RpcService{"CentrePlayerSceneService","%!s(func() string=0x11e0fe0)","%!s(func() string=0x11e1060)","%!s(func() string=0x11e10e0)",std::make_unique_for_overwrite<CentrePlayerSceneServiceImpl>()};
gMessageInfo[CentrePlayerSceneServiceSceneInfoC2SMessageId] = RpcService{"CentrePlayerSceneService","%!s(func() string=0x11e0fe0)","%!s(func() string=0x11e1060)","%!s(func() string=0x11e10e0)",std::make_unique_for_overwrite<CentrePlayerSceneServiceImpl>()};

gMessageInfo[CentrePlayerServiceTestMessageId] = RpcService{"CentrePlayerService","%!s(func() string=0x11e0fe0)","%!s(func() string=0x11e1060)","%!s(func() string=0x11e10e0)",std::make_unique_for_overwrite<CentrePlayerServiceImpl>()};

gMessageInfo[CentreSceneServiceRegisterSceneMessageId] = RpcService{"CentreSceneService","%!s(func() string=0x11e0fe0)","%!s(func() string=0x11e1060)","%!s(func() string=0x11e10e0)",std::make_unique_for_overwrite<CentreSceneServiceImpl>()};
gMessageInfo[CentreSceneServiceUnRegisterSceneMessageId] = RpcService{"CentreSceneService","%!s(func() string=0x11e0fe0)","%!s(func() string=0x11e1060)","%!s(func() string=0x11e10e0)",std::make_unique_for_overwrite<CentreSceneServiceImpl>()};

gMessageInfo[CentreServiceRegisterGameNodeMessageId] = RpcService{"CentreService","%!s(func() string=0x11e0fe0)","%!s(func() string=0x11e1060)","%!s(func() string=0x11e10e0)",std::make_unique_for_overwrite<CentreServiceImpl>()};
gMessageInfo[CentreServiceRegisterGateNodeMessageId] = RpcService{"CentreService","%!s(func() string=0x11e0fe0)","%!s(func() string=0x11e1060)","%!s(func() string=0x11e10e0)",std::make_unique_for_overwrite<CentreServiceImpl>()};
gMessageInfo[CentreServiceGatePlayerServiceMessageId] = RpcService{"CentreService","%!s(func() string=0x11e0fe0)","%!s(func() string=0x11e1060)","%!s(func() string=0x11e10e0)",std::make_unique_for_overwrite<CentreServiceImpl>()};
gMessageInfo[CentreServiceGateSessionDisconnectMessageId] = RpcService{"CentreService","%!s(func() string=0x11e0fe0)","%!s(func() string=0x11e1060)","%!s(func() string=0x11e10e0)",std::make_unique_for_overwrite<CentreServiceImpl>()};
gMessageInfo[CentreServiceLoginNodeAccountLoginMessageId] = RpcService{"CentreService","%!s(func() string=0x11e0fe0)","%!s(func() string=0x11e1060)","%!s(func() string=0x11e10e0)",std::make_unique_for_overwrite<CentreServiceImpl>()};
gMessageInfo[CentreServiceLoginNodeEnterGameMessageId] = RpcService{"CentreService","%!s(func() string=0x11e0fe0)","%!s(func() string=0x11e1060)","%!s(func() string=0x11e10e0)",std::make_unique_for_overwrite<CentreServiceImpl>()};
gMessageInfo[CentreServiceLoginNodeLeaveGameMessageId] = RpcService{"CentreService","%!s(func() string=0x11e0fe0)","%!s(func() string=0x11e1060)","%!s(func() string=0x11e10e0)",std::make_unique_for_overwrite<CentreServiceImpl>()};
gMessageInfo[CentreServiceLoginNodeSessionDisconnectMessageId] = RpcService{"CentreService","%!s(func() string=0x11e0fe0)","%!s(func() string=0x11e1060)","%!s(func() string=0x11e10e0)",std::make_unique_for_overwrite<CentreServiceImpl>()};
gMessageInfo[CentreServicePlayerServiceMessageId] = RpcService{"CentreService","%!s(func() string=0x11e0fe0)","%!s(func() string=0x11e1060)","%!s(func() string=0x11e10e0)",std::make_unique_for_overwrite<CentreServiceImpl>()};
gMessageInfo[CentreServiceEnterGsSucceedMessageId] = RpcService{"CentreService","%!s(func() string=0x11e0fe0)","%!s(func() string=0x11e1060)","%!s(func() string=0x11e10e0)",std::make_unique_for_overwrite<CentreServiceImpl>()};
gMessageInfo[CentreServiceRouteNodeStringMsgMessageId] = RpcService{"CentreService","%!s(func() string=0x11e0fe0)","%!s(func() string=0x11e1060)","%!s(func() string=0x11e10e0)",std::make_unique_for_overwrite<CentreServiceImpl>()};
gMessageInfo[CentreServiceRoutePlayerStringMsgMessageId] = RpcService{"CentreService","%!s(func() string=0x11e0fe0)","%!s(func() string=0x11e1060)","%!s(func() string=0x11e10e0)",std::make_unique_for_overwrite<CentreServiceImpl>()};
gMessageInfo[CentreServiceUnRegisterGameNodeMessageId] = RpcService{"CentreService","%!s(func() string=0x11e0fe0)","%!s(func() string=0x11e1060)","%!s(func() string=0x11e10e0)",std::make_unique_for_overwrite<CentreServiceImpl>()};

gMessageInfo[ClientPlayerSceneServiceEnterSceneMessageId] = RpcService{"ClientPlayerSceneService","%!s(func() string=0x11e0fe0)","%!s(func() string=0x11e1060)","%!s(func() string=0x11e10e0)",std::make_unique_for_overwrite<ClientPlayerSceneServiceImpl>()};
gClientToServerMessageId.emplace(ClientPlayerSceneServiceEnterSceneMessageId);
gMessageInfo[ClientPlayerSceneServiceNotifyEnterSceneMessageId] = RpcService{"ClientPlayerSceneService","%!s(func() string=0x11e0fe0)","%!s(func() string=0x11e1060)","%!s(func() string=0x11e10e0)",std::make_unique_for_overwrite<ClientPlayerSceneServiceImpl>()};
gClientToServerMessageId.emplace(ClientPlayerSceneServiceNotifyEnterSceneMessageId);
gMessageInfo[ClientPlayerSceneServiceSceneInfoC2SMessageId] = RpcService{"ClientPlayerSceneService","%!s(func() string=0x11e0fe0)","%!s(func() string=0x11e1060)","%!s(func() string=0x11e10e0)",std::make_unique_for_overwrite<ClientPlayerSceneServiceImpl>()};
gClientToServerMessageId.emplace(ClientPlayerSceneServiceSceneInfoC2SMessageId);
gMessageInfo[ClientPlayerSceneServiceNotifySceneInfoMessageId] = RpcService{"ClientPlayerSceneService","%!s(func() string=0x11e0fe0)","%!s(func() string=0x11e1060)","%!s(func() string=0x11e10e0)",std::make_unique_for_overwrite<ClientPlayerSceneServiceImpl>()};
gClientToServerMessageId.emplace(ClientPlayerSceneServiceNotifySceneInfoMessageId);
gMessageInfo[ClientPlayerSceneServiceNotifyActorCreateMessageId] = RpcService{"ClientPlayerSceneService","%!s(func() string=0x11e0fe0)","%!s(func() string=0x11e1060)","%!s(func() string=0x11e10e0)",std::make_unique_for_overwrite<ClientPlayerSceneServiceImpl>()};
gClientToServerMessageId.emplace(ClientPlayerSceneServiceNotifyActorCreateMessageId);
gMessageInfo[ClientPlayerSceneServiceNotifyActorDestroyMessageId] = RpcService{"ClientPlayerSceneService","%!s(func() string=0x11e0fe0)","%!s(func() string=0x11e1060)","%!s(func() string=0x11e10e0)",std::make_unique_for_overwrite<ClientPlayerSceneServiceImpl>()};
gClientToServerMessageId.emplace(ClientPlayerSceneServiceNotifyActorDestroyMessageId);
gMessageInfo[ClientPlayerSceneServiceNotifyActorListCreateMessageId] = RpcService{"ClientPlayerSceneService","%!s(func() string=0x11e0fe0)","%!s(func() string=0x11e1060)","%!s(func() string=0x11e10e0)",std::make_unique_for_overwrite<ClientPlayerSceneServiceImpl>()};
gClientToServerMessageId.emplace(ClientPlayerSceneServiceNotifyActorListCreateMessageId);
gMessageInfo[ClientPlayerSceneServiceNotifyActorListDestroyMessageId] = RpcService{"ClientPlayerSceneService","%!s(func() string=0x11e0fe0)","%!s(func() string=0x11e1060)","%!s(func() string=0x11e10e0)",std::make_unique_for_overwrite<ClientPlayerSceneServiceImpl>()};
gClientToServerMessageId.emplace(ClientPlayerSceneServiceNotifyActorListDestroyMessageId);

gMessageInfo[EntitySyncServiceSyncBaseAttributeMessageId] = RpcService{"EntitySyncService","%!s(func() string=0x11e0fe0)","%!s(func() string=0x11e1060)","%!s(func() string=0x11e10e0)",std::make_unique_for_overwrite<EntitySyncServiceImpl>()};
gClientToServerMessageId.emplace(EntitySyncServiceSyncBaseAttributeMessageId);
gMessageInfo[EntitySyncServiceSyncAttribute2FramesMessageId] = RpcService{"EntitySyncService","%!s(func() string=0x11e0fe0)","%!s(func() string=0x11e1060)","%!s(func() string=0x11e10e0)",std::make_unique_for_overwrite<EntitySyncServiceImpl>()};
gClientToServerMessageId.emplace(EntitySyncServiceSyncAttribute2FramesMessageId);
gMessageInfo[EntitySyncServiceSyncAttribute5FramesMessageId] = RpcService{"EntitySyncService","%!s(func() string=0x11e0fe0)","%!s(func() string=0x11e1060)","%!s(func() string=0x11e10e0)",std::make_unique_for_overwrite<EntitySyncServiceImpl>()};
gClientToServerMessageId.emplace(EntitySyncServiceSyncAttribute5FramesMessageId);
gMessageInfo[EntitySyncServiceSyncAttribute10FramesMessageId] = RpcService{"EntitySyncService","%!s(func() string=0x11e0fe0)","%!s(func() string=0x11e1060)","%!s(func() string=0x11e10e0)",std::make_unique_for_overwrite<EntitySyncServiceImpl>()};
gClientToServerMessageId.emplace(EntitySyncServiceSyncAttribute10FramesMessageId);
gMessageInfo[EntitySyncServiceSyncAttribute30FramesMessageId] = RpcService{"EntitySyncService","%!s(func() string=0x11e0fe0)","%!s(func() string=0x11e1060)","%!s(func() string=0x11e10e0)",std::make_unique_for_overwrite<EntitySyncServiceImpl>()};
gClientToServerMessageId.emplace(EntitySyncServiceSyncAttribute30FramesMessageId);
gMessageInfo[EntitySyncServiceSyncAttribute60FramesMessageId] = RpcService{"EntitySyncService","%!s(func() string=0x11e0fe0)","%!s(func() string=0x11e1060)","%!s(func() string=0x11e10e0)",std::make_unique_for_overwrite<EntitySyncServiceImpl>()};
gClientToServerMessageId.emplace(EntitySyncServiceSyncAttribute60FramesMessageId);

gMessageInfo[GamePlayerSceneServiceEnterSceneMessageId] = RpcService{"GamePlayerSceneService","%!s(func() string=0x11e0fe0)","%!s(func() string=0x11e1060)","%!s(func() string=0x11e10e0)",std::make_unique_for_overwrite<GamePlayerSceneServiceImpl>()};
gMessageInfo[GamePlayerSceneServiceLeaveSceneMessageId] = RpcService{"GamePlayerSceneService","%!s(func() string=0x11e0fe0)","%!s(func() string=0x11e1060)","%!s(func() string=0x11e10e0)",std::make_unique_for_overwrite<GamePlayerSceneServiceImpl>()};
gMessageInfo[GamePlayerSceneServiceEnterSceneS2CMessageId] = RpcService{"GamePlayerSceneService","%!s(func() string=0x11e0fe0)","%!s(func() string=0x11e1060)","%!s(func() string=0x11e10e0)",std::make_unique_for_overwrite<GamePlayerSceneServiceImpl>()};

gMessageInfo[GamePlayerServiceCentre2GsLoginMessageId] = RpcService{"GamePlayerService","%!s(func() string=0x11e0fe0)","%!s(func() string=0x11e1060)","%!s(func() string=0x11e10e0)",std::make_unique_for_overwrite<GamePlayerServiceImpl>()};
gMessageInfo[GamePlayerServiceExitGameMessageId] = RpcService{"GamePlayerService","%!s(func() string=0x11e0fe0)","%!s(func() string=0x11e1060)","%!s(func() string=0x11e10e0)",std::make_unique_for_overwrite<GamePlayerServiceImpl>()};

gMessageInfo[GameSceneServiceTestMessageId] = RpcService{"GameSceneService","%!s(func() string=0x11e0fe0)","%!s(func() string=0x11e1060)","%!s(func() string=0x11e10e0)",std::make_unique_for_overwrite<GameSceneServiceImpl>()};

gMessageInfo[GameServicePlayerEnterGameNodeMessageId] = RpcService{"GameService","%!s(func() string=0x11e0fe0)","%!s(func() string=0x11e1060)","%!s(func() string=0x11e10e0)",std::make_unique_for_overwrite<GameServiceImpl>()};
gMessageInfo[GameServiceSendMessageToPlayerMessageId] = RpcService{"GameService","%!s(func() string=0x11e0fe0)","%!s(func() string=0x11e1060)","%!s(func() string=0x11e10e0)",std::make_unique_for_overwrite<GameServiceImpl>()};
gMessageInfo[GameServiceClientSendMessageToPlayerMessageId] = RpcService{"GameService","%!s(func() string=0x11e0fe0)","%!s(func() string=0x11e1060)","%!s(func() string=0x11e10e0)",std::make_unique_for_overwrite<GameServiceImpl>()};
gMessageInfo[GameServiceRegisterGateNodeMessageId] = RpcService{"GameService","%!s(func() string=0x11e0fe0)","%!s(func() string=0x11e1060)","%!s(func() string=0x11e10e0)",std::make_unique_for_overwrite<GameServiceImpl>()};
gMessageInfo[GameServiceCentreSendToPlayerViaGameNodeMessageId] = RpcService{"GameService","%!s(func() string=0x11e0fe0)","%!s(func() string=0x11e1060)","%!s(func() string=0x11e10e0)",std::make_unique_for_overwrite<GameServiceImpl>()};
gMessageInfo[GameServiceInvokePlayerServiceMessageId] = RpcService{"GameService","%!s(func() string=0x11e0fe0)","%!s(func() string=0x11e1060)","%!s(func() string=0x11e10e0)",std::make_unique_for_overwrite<GameServiceImpl>()};
gMessageInfo[GameServiceRouteNodeStringMsgMessageId] = RpcService{"GameService","%!s(func() string=0x11e0fe0)","%!s(func() string=0x11e1060)","%!s(func() string=0x11e10e0)",std::make_unique_for_overwrite<GameServiceImpl>()};
gMessageInfo[GameServiceRoutePlayerStringMsgMessageId] = RpcService{"GameService","%!s(func() string=0x11e0fe0)","%!s(func() string=0x11e1060)","%!s(func() string=0x11e10e0)",std::make_unique_for_overwrite<GameServiceImpl>()};
gMessageInfo[GameServiceUpdateSessionDetailMessageId] = RpcService{"GameService","%!s(func() string=0x11e0fe0)","%!s(func() string=0x11e1060)","%!s(func() string=0x11e10e0)",std::make_unique_for_overwrite<GameServiceImpl>()};
gMessageInfo[GameServiceEnterSceneMessageId] = RpcService{"GameService","%!s(func() string=0x11e0fe0)","%!s(func() string=0x11e1060)","%!s(func() string=0x11e10e0)",std::make_unique_for_overwrite<GameServiceImpl>()};
gMessageInfo[GameServiceCreateSceneMessageId] = RpcService{"GameService","%!s(func() string=0x11e0fe0)","%!s(func() string=0x11e1060)","%!s(func() string=0x11e10e0)",std::make_unique_for_overwrite<GameServiceImpl>()};

gMessageInfo[GateServiceRegisterGameMessageId] = RpcService{"GateService","%!s(func() string=0x11e0fe0)","%!s(func() string=0x11e1060)","%!s(func() string=0x11e10e0)",std::make_unique_for_overwrite<GateServiceImpl>()};
gMessageInfo[GateServiceUnRegisterGameMessageId] = RpcService{"GateService","%!s(func() string=0x11e0fe0)","%!s(func() string=0x11e1060)","%!s(func() string=0x11e10e0)",std::make_unique_for_overwrite<GateServiceImpl>()};
gMessageInfo[GateServicePlayerEnterGameNodeMessageId] = RpcService{"GateService","%!s(func() string=0x11e0fe0)","%!s(func() string=0x11e1060)","%!s(func() string=0x11e10e0)",std::make_unique_for_overwrite<GateServiceImpl>()};
gMessageInfo[GateServiceSendMessageToPlayerMessageId] = RpcService{"GateService","%!s(func() string=0x11e0fe0)","%!s(func() string=0x11e1060)","%!s(func() string=0x11e10e0)",std::make_unique_for_overwrite<GateServiceImpl>()};
gMessageInfo[GateServiceKickSessionByCentreMessageId] = RpcService{"GateService","%!s(func() string=0x11e0fe0)","%!s(func() string=0x11e1060)","%!s(func() string=0x11e10e0)",std::make_unique_for_overwrite<GateServiceImpl>()};
gMessageInfo[GateServiceRouteNodeMessageMessageId] = RpcService{"GateService","%!s(func() string=0x11e0fe0)","%!s(func() string=0x11e1060)","%!s(func() string=0x11e10e0)",std::make_unique_for_overwrite<GateServiceImpl>()};
gMessageInfo[GateServiceRoutePlayerMessageMessageId] = RpcService{"GateService","%!s(func() string=0x11e0fe0)","%!s(func() string=0x11e1060)","%!s(func() string=0x11e10e0)",std::make_unique_for_overwrite<GateServiceImpl>()};
gMessageInfo[GateServiceBroadcastToPlayersMessageId] = RpcService{"GateService","%!s(func() string=0x11e0fe0)","%!s(func() string=0x11e1060)","%!s(func() string=0x11e10e0)",std::make_unique_for_overwrite<GateServiceImpl>()};

gMessageInfo[PlayerClientCommonServiceSendTipToClientMessageId] = RpcService{"PlayerClientCommonService","%!s(func() string=0x11e0fe0)","%!s(func() string=0x11e1060)","%!s(func() string=0x11e10e0)",std::make_unique_for_overwrite<PlayerClientCommonServiceImpl>()};
gClientToServerMessageId.emplace(PlayerClientCommonServiceSendTipToClientMessageId);
gMessageInfo[PlayerClientCommonServiceKickPlayerMessageId] = RpcService{"PlayerClientCommonService","%!s(func() string=0x11e0fe0)","%!s(func() string=0x11e1060)","%!s(func() string=0x11e10e0)",std::make_unique_for_overwrite<PlayerClientCommonServiceImpl>()};
gClientToServerMessageId.emplace(PlayerClientCommonServiceKickPlayerMessageId);

gMessageInfo[PlayerSkillServiceReleaseSkillMessageId] = RpcService{"PlayerSkillService","%!s(func() string=0x11e0fe0)","%!s(func() string=0x11e1060)","%!s(func() string=0x11e10e0)",std::make_unique_for_overwrite<PlayerSkillServiceImpl>()};
gClientToServerMessageId.emplace(PlayerSkillServiceReleaseSkillMessageId);
gMessageInfo[PlayerSkillServiceNotifySkillUsedMessageId] = RpcService{"PlayerSkillService","%!s(func() string=0x11e0fe0)","%!s(func() string=0x11e1060)","%!s(func() string=0x11e10e0)",std::make_unique_for_overwrite<PlayerSkillServiceImpl>()};
gClientToServerMessageId.emplace(PlayerSkillServiceNotifySkillUsedMessageId);
gMessageInfo[PlayerSkillServiceNotifySkillInterruptedMessageId] = RpcService{"PlayerSkillService","%!s(func() string=0x11e0fe0)","%!s(func() string=0x11e1060)","%!s(func() string=0x11e10e0)",std::make_unique_for_overwrite<PlayerSkillServiceImpl>()};
gClientToServerMessageId.emplace(PlayerSkillServiceNotifySkillInterruptedMessageId);
gMessageInfo[PlayerSkillServiceGetSkillListMessageId] = RpcService{"PlayerSkillService","%!s(func() string=0x11e0fe0)","%!s(func() string=0x11e1060)","%!s(func() string=0x11e10e0)",std::make_unique_for_overwrite<PlayerSkillServiceImpl>()};
gClientToServerMessageId.emplace(PlayerSkillServiceGetSkillListMessageId);

}
