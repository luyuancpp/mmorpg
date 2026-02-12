#include <array>
#include "service_metadata.h"
#include "proto/common/base/node.pb.h"

#include "proto/etcd/etcd.grpc.pb.h"
#include "proto/etcd/etcd.grpc.pb.h"
#include "proto/etcd/etcd.grpc.pb.h"
#include "proto/login/login.grpc.pb.h"
#include "proto/centre/centre_player.pb.h"
#include "proto/centre/centre_player_scene.pb.h"
#include "proto/centre/centre_scene.pb.h"
#include "proto/centre/centre_service.pb.h"
#include "proto/gate/gate_service.pb.h"
#include "proto/room/game_client_player.pb.h"
#include "proto/room/game_player.pb.h"
#include "proto/room/game_player_scene.pb.h"
#include "proto/room/game_scene.pb.h"
#include "proto/room/player_scene.pb.h"
#include "proto/room/player_skill.pb.h"
#include "proto/room/player_state_attribute_sync.pb.h"
#include "proto/room/scene.pb.h"

#include "rpc/service_metadata/etcd_service_metadata.h"
#include "rpc/service_metadata/etcd_service_metadata.h"
#include "rpc/service_metadata/etcd_service_metadata.h"
#include "rpc/service_metadata/login_service_metadata.h"
#include "rpc/service_metadata/centre_player_service_metadata.h"
#include "rpc/service_metadata/centre_player_scene_service_metadata.h"
#include "rpc/service_metadata/centre_scene_service_metadata.h"
#include "rpc/service_metadata/centre_service_service_metadata.h"
#include "rpc/service_metadata/gate_service_service_metadata.h"
#include "rpc/service_metadata/game_client_player_service_metadata.h"
#include "rpc/service_metadata/game_player_service_metadata.h"
#include "rpc/service_metadata/game_player_scene_service_metadata.h"
#include "rpc/service_metadata/game_scene_service_metadata.h"
#include "rpc/service_metadata/player_scene_service_metadata.h"
#include "rpc/service_metadata/player_skill_service_metadata.h"
#include "rpc/service_metadata/player_state_attribute_sync_service_metadata.h"
#include "rpc/service_metadata/scene_service_metadata.h"


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

std::unordered_set<uint32_t> gClientMessageIdWhitelist;
std::array<RpcService, 85> gRpcServiceRegistry;

void InitMessageInfo()
{
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
    gRpcServiceRegistry[ClientPlayerLoginLoginMessageId] = RpcService{"ClientPlayerLogin", "Login", std::make_unique_for_overwrite<::loginpb::LoginRequest>(), std::make_unique_for_overwrite<::loginpb::LoginResponse>(), nullptr, 1, eNodeType::LoginNodeService, loginpb::SendClientPlayerLoginLogin};
    gRpcServiceRegistry[ClientPlayerLoginCreatePlayerMessageId] = RpcService{"ClientPlayerLogin", "CreatePlayer", std::make_unique_for_overwrite<::loginpb::CreatePlayerRequest>(), std::make_unique_for_overwrite<::loginpb::CreatePlayerResponse>(), nullptr, 1, eNodeType::LoginNodeService, loginpb::SendClientPlayerLoginCreatePlayer};
    gRpcServiceRegistry[ClientPlayerLoginEnterGameMessageId] = RpcService{"ClientPlayerLogin", "EnterGame", std::make_unique_for_overwrite<::loginpb::EnterGameRequest>(), std::make_unique_for_overwrite<::loginpb::EnterGameResponse>(), nullptr, 1, eNodeType::LoginNodeService, loginpb::SendClientPlayerLoginEnterGame};
    gRpcServiceRegistry[ClientPlayerLoginLeaveGameMessageId] = RpcService{"ClientPlayerLogin", "LeaveGame", std::make_unique_for_overwrite<::loginpb::LeaveGameRequest>(), std::make_unique_for_overwrite<::loginpb::LoginEmptyResponse>(), nullptr, 1, eNodeType::LoginNodeService, loginpb::SendClientPlayerLoginLeaveGame};
    gRpcServiceRegistry[ClientPlayerLoginDisconnectMessageId] = RpcService{"ClientPlayerLogin", "Disconnect", std::make_unique_for_overwrite<::loginpb::LoginNodeDisconnectRequest>(), std::make_unique_for_overwrite<::loginpb::LoginEmptyResponse>(), nullptr, 1, eNodeType::LoginNodeService, loginpb::SendClientPlayerLoginDisconnect};
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
    gRpcServiceRegistry[GatePlayerEnterGameNodeMessageId] = RpcService{"Gate", "PlayerEnterGameNode", std::make_unique_for_overwrite<::RegisterGameNodeSessionRequest>(), std::make_unique_for_overwrite<::RegisterGameNodeSessionResponse>(), std::make_unique_for_overwrite<GateImpl>(), 0, eNodeType::GateNodeService};
    gRpcServiceRegistry[GateSendMessageToPlayerMessageId] = RpcService{"Gate", "SendMessageToPlayer", std::make_unique_for_overwrite<::NodeRouteMessageRequest>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<GateImpl>(), 0, eNodeType::GateNodeService};
    gRpcServiceRegistry[GateKickSessionByCentreMessageId] = RpcService{"Gate", "KickSessionByCentre", std::make_unique_for_overwrite<::KickSessionRequest>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<GateImpl>(), 0, eNodeType::GateNodeService};
    gRpcServiceRegistry[GateRouteNodeMessageMessageId] = RpcService{"Gate", "RouteNodeMessage", std::make_unique_for_overwrite<::RouteMessageRequest>(), std::make_unique_for_overwrite<::RouteMessageResponse>(), std::make_unique_for_overwrite<GateImpl>(), 0, eNodeType::GateNodeService};
    gRpcServiceRegistry[GateRoutePlayerMessageMessageId] = RpcService{"Gate", "RoutePlayerMessage", std::make_unique_for_overwrite<::RoutePlayerMessageRequest>(), std::make_unique_for_overwrite<::RoutePlayerMessageResponse>(), std::make_unique_for_overwrite<GateImpl>(), 0, eNodeType::GateNodeService};
    gRpcServiceRegistry[GateBroadcastToPlayersMessageId] = RpcService{"Gate", "BroadcastToPlayers", std::make_unique_for_overwrite<::BroadcastToPlayersRequest>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<GateImpl>(), 0, eNodeType::GateNodeService};
    gRpcServiceRegistry[GateNodeHandshakeMessageId] = RpcService{"Gate", "NodeHandshake", std::make_unique_for_overwrite<::NodeHandshakeRequest>(), std::make_unique_for_overwrite<::NodeHandshakeResponse>(), std::make_unique_for_overwrite<GateImpl>(), 0, eNodeType::GateNodeService};
    gRpcServiceRegistry[GateBindSessionToGateMessageId] = RpcService{"Gate", "BindSessionToGate", std::make_unique_for_overwrite<::BindSessionToGateRequest>(), std::make_unique_for_overwrite<::BindSessionToGateResponse>(), std::make_unique_for_overwrite<GateImpl>(), 0, eNodeType::GateNodeService};
    gRpcServiceRegistry[SceneClientPlayerCommonSendTipToClientMessageId] = RpcService{"SceneClientPlayerCommon", "SendTipToClient", std::make_unique_for_overwrite<::TipInfoMessage>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<SceneClientPlayerCommonImpl>(), 0, eNodeType::RoomNodeService};
    gRpcServiceRegistry[SceneClientPlayerCommonKickPlayerMessageId] = RpcService{"SceneClientPlayerCommon", "KickPlayer", std::make_unique_for_overwrite<::GameKickPlayerRequest>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<SceneClientPlayerCommonImpl>(), 0, eNodeType::RoomNodeService};
    gRpcServiceRegistry[ScenePlayerCentre2GsLoginMessageId] = RpcService{"ScenePlayer", "Centre2GsLogin", std::make_unique_for_overwrite<::Centre2GsLoginRequest>(), std::make_unique_for_overwrite<::google::protobuf::Empty>(), std::make_unique_for_overwrite<ScenePlayerImpl>(), 0, eNodeType::RoomNodeService};
    gRpcServiceRegistry[ScenePlayerExitGameMessageId] = RpcService{"ScenePlayer", "ExitGame", std::make_unique_for_overwrite<::GameNodeExitGameRequest>(), std::make_unique_for_overwrite<::google::protobuf::Empty>(), std::make_unique_for_overwrite<ScenePlayerImpl>(), 0, eNodeType::RoomNodeService};
    gRpcServiceRegistry[SceneScenePlayerEnterSceneMessageId] = RpcService{"SceneScenePlayer", "EnterScene", std::make_unique_for_overwrite<::GsEnterSceneRequest>(), std::make_unique_for_overwrite<::google::protobuf::Empty>(), std::make_unique_for_overwrite<SceneScenePlayerImpl>(), 0, eNodeType::RoomNodeService};
    gRpcServiceRegistry[SceneScenePlayerLeaveSceneMessageId] = RpcService{"SceneScenePlayer", "LeaveScene", std::make_unique_for_overwrite<::GsLeaveSceneRequest>(), std::make_unique_for_overwrite<::google::protobuf::Empty>(), std::make_unique_for_overwrite<SceneScenePlayerImpl>(), 0, eNodeType::RoomNodeService};
    gRpcServiceRegistry[SceneScenePlayerEnterSceneS2CMessageId] = RpcService{"SceneScenePlayer", "EnterSceneS2C", std::make_unique_for_overwrite<::EnterSceneS2CRequest>(), std::make_unique_for_overwrite<::EnterScenerS2CResponse>(), std::make_unique_for_overwrite<SceneScenePlayerImpl>(), 0, eNodeType::RoomNodeService};
    gRpcServiceRegistry[SceneSceneTestMessageId] = RpcService{"SceneScene", "Test", std::make_unique_for_overwrite<::GameSceneTest>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<SceneSceneImpl>(), 0, eNodeType::RoomNodeService};
    gRpcServiceRegistry[SceneSceneClientPlayerEnterSceneMessageId] = RpcService{"SceneSceneClientPlayer", "EnterScene", std::make_unique_for_overwrite<::EnterSceneC2SRequest>(), std::make_unique_for_overwrite<::EnterSceneC2SResponse>(), std::make_unique_for_overwrite<SceneSceneClientPlayerImpl>(), 0, eNodeType::RoomNodeService};
    gRpcServiceRegistry[SceneSceneClientPlayerNotifyEnterSceneMessageId] = RpcService{"SceneSceneClientPlayer", "NotifyEnterScene", std::make_unique_for_overwrite<::EnterSceneS2C>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<SceneSceneClientPlayerImpl>(), 0, eNodeType::RoomNodeService};
    gRpcServiceRegistry[SceneSceneClientPlayerSceneInfoC2SMessageId] = RpcService{"SceneSceneClientPlayer", "SceneInfoC2S", std::make_unique_for_overwrite<::SceneInfoRequest>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<SceneSceneClientPlayerImpl>(), 0, eNodeType::RoomNodeService};
    gRpcServiceRegistry[SceneSceneClientPlayerNotifySceneInfoMessageId] = RpcService{"SceneSceneClientPlayer", "NotifySceneInfo", std::make_unique_for_overwrite<::SceneInfoS2C>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<SceneSceneClientPlayerImpl>(), 0, eNodeType::RoomNodeService};
    gRpcServiceRegistry[SceneSceneClientPlayerNotifyActorCreateMessageId] = RpcService{"SceneSceneClientPlayer", "NotifyActorCreate", std::make_unique_for_overwrite<::ActorCreateS2C>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<SceneSceneClientPlayerImpl>(), 0, eNodeType::RoomNodeService};
    gRpcServiceRegistry[SceneSceneClientPlayerNotifyActorDestroyMessageId] = RpcService{"SceneSceneClientPlayer", "NotifyActorDestroy", std::make_unique_for_overwrite<::ActorDestroyS2C>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<SceneSceneClientPlayerImpl>(), 0, eNodeType::RoomNodeService};
    gRpcServiceRegistry[SceneSceneClientPlayerNotifyActorListCreateMessageId] = RpcService{"SceneSceneClientPlayer", "NotifyActorListCreate", std::make_unique_for_overwrite<::ActorListCreateS2C>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<SceneSceneClientPlayerImpl>(), 0, eNodeType::RoomNodeService};
    gRpcServiceRegistry[SceneSceneClientPlayerNotifyActorListDestroyMessageId] = RpcService{"SceneSceneClientPlayer", "NotifyActorListDestroy", std::make_unique_for_overwrite<::ActorListDestroyS2C>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<SceneSceneClientPlayerImpl>(), 0, eNodeType::RoomNodeService};
    gRpcServiceRegistry[SceneSkillClientPlayerReleaseSkillMessageId] = RpcService{"SceneSkillClientPlayer", "ReleaseSkill", std::make_unique_for_overwrite<::ReleaseSkillSkillRequest>(), std::make_unique_for_overwrite<::ReleaseSkillSkillResponse>(), std::make_unique_for_overwrite<SceneSkillClientPlayerImpl>(), 0, eNodeType::RoomNodeService};
    gRpcServiceRegistry[SceneSkillClientPlayerNotifySkillUsedMessageId] = RpcService{"SceneSkillClientPlayer", "NotifySkillUsed", std::make_unique_for_overwrite<::SkillUsedS2C>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<SceneSkillClientPlayerImpl>(), 0, eNodeType::RoomNodeService};
    gRpcServiceRegistry[SceneSkillClientPlayerNotifySkillInterruptedMessageId] = RpcService{"SceneSkillClientPlayer", "NotifySkillInterrupted", std::make_unique_for_overwrite<::SkillInterruptedS2C>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<SceneSkillClientPlayerImpl>(), 0, eNodeType::RoomNodeService};
    gRpcServiceRegistry[SceneSkillClientPlayerGetSkillListMessageId] = RpcService{"SceneSkillClientPlayer", "GetSkillList", std::make_unique_for_overwrite<::GetSkillListRequest>(), std::make_unique_for_overwrite<::GetSkillListResponse>(), std::make_unique_for_overwrite<SceneSkillClientPlayerImpl>(), 0, eNodeType::RoomNodeService};
    gRpcServiceRegistry[ScenePlayerSyncSyncBaseAttributeMessageId] = RpcService{"ScenePlayerSync", "SyncBaseAttribute", std::make_unique_for_overwrite<::ActorBaseAttributesS2C>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<ScenePlayerSyncImpl>(), 0, eNodeType::RoomNodeService};
    gRpcServiceRegistry[ScenePlayerSyncSyncAttribute2FramesMessageId] = RpcService{"ScenePlayerSync", "SyncAttribute2Frames", std::make_unique_for_overwrite<::AttributeDelta2FramesS2C>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<ScenePlayerSyncImpl>(), 0, eNodeType::RoomNodeService};
    gRpcServiceRegistry[ScenePlayerSyncSyncAttribute5FramesMessageId] = RpcService{"ScenePlayerSync", "SyncAttribute5Frames", std::make_unique_for_overwrite<::AttributeDelta5FramesS2C>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<ScenePlayerSyncImpl>(), 0, eNodeType::RoomNodeService};
    gRpcServiceRegistry[ScenePlayerSyncSyncAttribute10FramesMessageId] = RpcService{"ScenePlayerSync", "SyncAttribute10Frames", std::make_unique_for_overwrite<::AttributeDelta10FramesS2C>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<ScenePlayerSyncImpl>(), 0, eNodeType::RoomNodeService};
    gRpcServiceRegistry[ScenePlayerSyncSyncAttribute30FramesMessageId] = RpcService{"ScenePlayerSync", "SyncAttribute30Frames", std::make_unique_for_overwrite<::AttributeDelta30FramesS2C>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<ScenePlayerSyncImpl>(), 0, eNodeType::RoomNodeService};
    gRpcServiceRegistry[ScenePlayerSyncSyncAttribute60FramesMessageId] = RpcService{"ScenePlayerSync", "SyncAttribute60Frames", std::make_unique_for_overwrite<::AttributeDelta60FramesS2C>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<ScenePlayerSyncImpl>(), 0, eNodeType::RoomNodeService};
    gRpcServiceRegistry[ScenePlayerEnterGameNodeMessageId] = RpcService{"Scene", "PlayerEnterGameNode", std::make_unique_for_overwrite<::PlayerEnterGameNodeRequest>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<SceneImpl>(), 0, eNodeType::RoomNodeService};
    gRpcServiceRegistry[SceneSendMessageToPlayerMessageId] = RpcService{"Scene", "SendMessageToPlayer", std::make_unique_for_overwrite<::NodeRouteMessageRequest>(), std::make_unique_for_overwrite<::NodeRouteMessageResponse>(), std::make_unique_for_overwrite<SceneImpl>(), 0, eNodeType::RoomNodeService};
    gRpcServiceRegistry[SceneProcessClientPlayerMessageMessageId] = RpcService{"Scene", "ProcessClientPlayerMessage", std::make_unique_for_overwrite<::ProcessClientPlayerMessageRequest>(), std::make_unique_for_overwrite<::ProcessClientPlayerMessageResponse>(), std::make_unique_for_overwrite<SceneImpl>(), 0, eNodeType::RoomNodeService};
    gRpcServiceRegistry[SceneCentreSendToPlayerViaGameNodeMessageId] = RpcService{"Scene", "CentreSendToPlayerViaGameNode", std::make_unique_for_overwrite<::NodeRouteMessageRequest>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<SceneImpl>(), 0, eNodeType::RoomNodeService};
    gRpcServiceRegistry[SceneInvokePlayerServiceMessageId] = RpcService{"Scene", "InvokePlayerService", std::make_unique_for_overwrite<::NodeRouteMessageRequest>(), std::make_unique_for_overwrite<::NodeRouteMessageResponse>(), std::make_unique_for_overwrite<SceneImpl>(), 0, eNodeType::RoomNodeService};
    gRpcServiceRegistry[SceneRouteNodeStringMsgMessageId] = RpcService{"Scene", "RouteNodeStringMsg", std::make_unique_for_overwrite<::RouteMessageRequest>(), std::make_unique_for_overwrite<::RouteMessageResponse>(), std::make_unique_for_overwrite<SceneImpl>(), 0, eNodeType::RoomNodeService};
    gRpcServiceRegistry[SceneRoutePlayerStringMsgMessageId] = RpcService{"Scene", "RoutePlayerStringMsg", std::make_unique_for_overwrite<::RoutePlayerMessageRequest>(), std::make_unique_for_overwrite<::RoutePlayerMessageResponse>(), std::make_unique_for_overwrite<SceneImpl>(), 0, eNodeType::RoomNodeService};
    gRpcServiceRegistry[SceneUpdateSessionDetailMessageId] = RpcService{"Scene", "UpdateSessionDetail", std::make_unique_for_overwrite<::RegisterPlayerSessionRequest>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<SceneImpl>(), 0, eNodeType::RoomNodeService};
    gRpcServiceRegistry[SceneEnterSceneMessageId] = RpcService{"Scene", "EnterScene", std::make_unique_for_overwrite<::Centre2GsEnterSceneRequest>(), std::make_unique_for_overwrite<::Empty>(), std::make_unique_for_overwrite<SceneImpl>(), 0, eNodeType::RoomNodeService};
    gRpcServiceRegistry[SceneCreateSceneMessageId] = RpcService{"Scene", "CreateScene", std::make_unique_for_overwrite<::CreateSceneRequest>(), std::make_unique_for_overwrite<::CreateSceneResponse>(), std::make_unique_for_overwrite<SceneImpl>(), 0, eNodeType::RoomNodeService};
    gRpcServiceRegistry[SceneNodeHandshakeMessageId] = RpcService{"Scene", "NodeHandshake", std::make_unique_for_overwrite<::NodeHandshakeRequest>(), std::make_unique_for_overwrite<::NodeHandshakeResponse>(), std::make_unique_for_overwrite<SceneImpl>(), 0, eNodeType::RoomNodeService};


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
