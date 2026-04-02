#include <array>
#include "rpc_event_registry.h"
#include "proto/common/base/node.pb.h"
#include "thread_context/dispatcher_manager.h"

#include "proto/chat/chat.pb.h"
#include "proto/data_service/data_service.pb.h"
#include "proto/etcd/etcd.pb.h"
#include "proto/friend/friend.pb.h"
#include "proto/gate/gate_service.pb.h"
#include "proto/guild/guild.pb.h"
#include "proto/login/login.pb.h"
#include "proto/scene/game_client_player.pb.h"
#include "proto/scene/game_player.pb.h"
#include "proto/scene/game_player_scene.pb.h"
#include "proto/scene/game_scene.pb.h"
#include "proto/scene/player_currency.pb.h"
#include "proto/scene/player_rollback.pb.h"
#include "proto/scene/player_scene.pb.h"
#include "proto/scene/player_skill.pb.h"
#include "proto/scene/player_state_attribute_sync.pb.h"
#include "proto/scene/scene.pb.h"
#include "proto/scene_manager/scene_manager_service.pb.h"
#include "proto/slg/slg_battle.pb.h"
#include "proto/slg/slg_map.pb.h"
#include "proto/turnbased/turnbased_battle.pb.h"
#include "proto/turnbased/turnbased_dungeon.pb.h"

#include "rpc/service_metadata/chat_service_metadata.h"
#include "rpc/service_metadata/data_service_service_metadata.h"
#include "rpc/service_metadata/etcd_service_metadata.h"
#include "rpc/service_metadata/friend_service_metadata.h"
#include "rpc/service_metadata/gate_service_service_metadata.h"
#include "rpc/service_metadata/guild_service_metadata.h"
#include "rpc/service_metadata/login_service_metadata.h"
#include "rpc/service_metadata/game_client_player_service_metadata.h"
#include "rpc/service_metadata/game_player_service_metadata.h"
#include "rpc/service_metadata/game_player_scene_service_metadata.h"
#include "rpc/service_metadata/game_scene_service_metadata.h"
#include "rpc/service_metadata/player_currency_service_metadata.h"
#include "rpc/service_metadata/player_rollback_service_metadata.h"
#include "rpc/service_metadata/player_scene_service_metadata.h"
#include "rpc/service_metadata/player_skill_service_metadata.h"
#include "rpc/service_metadata/player_state_attribute_sync_service_metadata.h"
#include "rpc/service_metadata/scene_service_metadata.h"
#include "rpc/service_metadata/scene_manager_service_service_metadata.h"
#include "rpc/service_metadata/slg_battle_service_metadata.h"
#include "rpc/service_metadata/slg_map_service_metadata.h"
#include "rpc/service_metadata/turnbased_battle_service_metadata.h"
#include "rpc/service_metadata/turnbased_dungeon_service_metadata.h"

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
class SceneCurrencyClientPlayerImpl final : public SceneCurrencyClientPlayer {};
class SceneRollbackClientPlayerImpl final : public SceneRollbackClientPlayer {};
class SceneSceneClientPlayerImpl final : public SceneSceneClientPlayer {};
class SceneSkillClientPlayerImpl final : public SceneSkillClientPlayer {};
class ScenePlayerSyncImpl final : public ScenePlayerSync {};
class SceneImpl final : public Scene {};
class SlgBattleImpl final : public SlgBattle {};
class SlgMapImpl final : public SlgMap {};
class TurnBasedBattleImpl final : public TurnBasedBattle {};
class TurnBasedDungeonImpl final : public TurnBasedDungeon {};

namespace chatpb{void SendClientPlayerChatSendChat(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace chatpb{void SendClientPlayerChatPullChatHistory(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace data_service{void SendDataServiceLoadPlayerData(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace data_service{void SendDataServiceSavePlayerData(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace data_service{void SendDataServiceGetPlayerField(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace data_service{void SendDataServiceSetPlayerField(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace data_service{void SendDataServiceRegisterPlayerZone(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace data_service{void SendDataServiceGetPlayerHomeZone(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace data_service{void SendDataServiceBatchGetPlayerHomeZone(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace data_service{void SendDataServiceDeletePlayerData(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace data_service{void SendDataServiceCreatePlayerSnapshot(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace data_service{void SendDataServiceListPlayerSnapshots(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace data_service{void SendDataServiceGetPlayerSnapshotDiff(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace data_service{void SendDataServiceRollbackPlayer(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace data_service{void SendDataServiceRollbackZone(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace data_service{void SendDataServiceRollbackAll(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace data_service{void SendDataServiceBatchRecallItems(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace data_service{void SendDataServiceQueryTransactionLog(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace data_service{void SendDataServiceCreateEventSnapshot(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
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
namespace friendpb{void SendFriendServiceAddFriend(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace friendpb{void SendFriendServiceAcceptFriend(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace friendpb{void SendFriendServiceRejectFriend(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace friendpb{void SendFriendServiceRemoveFriend(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace friendpb{void SendFriendServiceGetFriendList(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace friendpb{void SendFriendServiceGetPendingRequests(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace guildpb{void SendGuildServiceCreateGuild(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace guildpb{void SendGuildServiceGetGuild(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace guildpb{void SendGuildServiceGetPlayerGuild(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace guildpb{void SendGuildServiceJoinGuild(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace guildpb{void SendGuildServiceLeaveGuild(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace guildpb{void SendGuildServiceDisbandGuild(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace guildpb{void SendGuildServiceSetAnnouncement(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace guildpb{void SendGuildServiceUpdateGuildScore(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace guildpb{void SendGuildServiceGetGuildRank(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace guildpb{void SendGuildServiceGetGuildRankByGuild(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace loginpb{void SendClientPlayerLoginLogin(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace loginpb{void SendClientPlayerLoginCreatePlayer(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace loginpb{void SendClientPlayerLoginEnterGame(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace loginpb{void SendClientPlayerLoginLeaveGame(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace loginpb{void SendClientPlayerLoginDisconnect(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace loginpb{void SendLoginPreGateAssignGate(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace loginpb{void SendLoginAdminRemovePlayersFromAccounts(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace scene_manager{void SendSceneManagerCreateScene(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace scene_manager{void SendSceneManagerDestroyScene(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace scene_manager{void SendSceneManagerEnterScene(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}
namespace scene_manager{void SendSceneManagerLeaveScene(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}

std::array<RpcMethodMeta, 137> gRpcMethodRegistry;

void InitMessageInfo()
{
    // --- ClientPlayerChat ---
    gRpcMethodRegistry[ClientPlayerChatSendChatMessageId] = RpcMethodMeta{
        "ClientPlayerChat", "SendChat",
        std::make_unique<::chatpb::SendChatRequest>(),
        std::make_unique<::chatpb::SendChatResponse>(),
        nullptr, 0, common::base::eNodeType::ChatNodeService, chatpb::SendClientPlayerChatSendChat};
    gRpcMethodRegistry[ClientPlayerChatPullChatHistoryMessageId] = RpcMethodMeta{
        "ClientPlayerChat", "PullChatHistory",
        std::make_unique<::chatpb::PullChatHistoryRequest>(),
        std::make_unique<::chatpb::PullChatHistoryResponse>(),
        nullptr, 0, common::base::eNodeType::ChatNodeService, chatpb::SendClientPlayerChatPullChatHistory};

    // --- DataService ---
    gRpcMethodRegistry[DataServiceLoadPlayerDataMessageId] = RpcMethodMeta{
        "DataService", "LoadPlayerData",
        std::make_unique<::data_service::LoadPlayerDataRequest>(),
        std::make_unique<::data_service::LoadPlayerDataResponse>(),
        nullptr, 0, common::base::eNodeType::DataServiceNodeService, data_service::SendDataServiceLoadPlayerData};
    gRpcMethodRegistry[DataServiceSavePlayerDataMessageId] = RpcMethodMeta{
        "DataService", "SavePlayerData",
        std::make_unique<::data_service::SavePlayerDataRequest>(),
        std::make_unique<::data_service::SavePlayerDataResponse>(),
        nullptr, 0, common::base::eNodeType::DataServiceNodeService, data_service::SendDataServiceSavePlayerData};
    gRpcMethodRegistry[DataServiceGetPlayerFieldMessageId] = RpcMethodMeta{
        "DataService", "GetPlayerField",
        std::make_unique<::data_service::GetPlayerFieldRequest>(),
        std::make_unique<::data_service::GetPlayerFieldResponse>(),
        nullptr, 0, common::base::eNodeType::DataServiceNodeService, data_service::SendDataServiceGetPlayerField};
    gRpcMethodRegistry[DataServiceSetPlayerFieldMessageId] = RpcMethodMeta{
        "DataService", "SetPlayerField",
        std::make_unique<::data_service::SetPlayerFieldRequest>(),
        std::make_unique<::data_service::SetPlayerFieldResponse>(),
        nullptr, 0, common::base::eNodeType::DataServiceNodeService, data_service::SendDataServiceSetPlayerField};
    gRpcMethodRegistry[DataServiceRegisterPlayerZoneMessageId] = RpcMethodMeta{
        "DataService", "RegisterPlayerZone",
        std::make_unique<::data_service::RegisterPlayerZoneRequest>(),
        std::make_unique<::google::protobuf::Empty>(),
        nullptr, 0, common::base::eNodeType::DataServiceNodeService, data_service::SendDataServiceRegisterPlayerZone};
    gRpcMethodRegistry[DataServiceGetPlayerHomeZoneMessageId] = RpcMethodMeta{
        "DataService", "GetPlayerHomeZone",
        std::make_unique<::data_service::GetPlayerHomeZoneRequest>(),
        std::make_unique<::data_service::GetPlayerHomeZoneResponse>(),
        nullptr, 0, common::base::eNodeType::DataServiceNodeService, data_service::SendDataServiceGetPlayerHomeZone};
    gRpcMethodRegistry[DataServiceBatchGetPlayerHomeZoneMessageId] = RpcMethodMeta{
        "DataService", "BatchGetPlayerHomeZone",
        std::make_unique<::data_service::BatchGetPlayerHomeZoneRequest>(),
        std::make_unique<::data_service::BatchGetPlayerHomeZoneResponse>(),
        nullptr, 0, common::base::eNodeType::DataServiceNodeService, data_service::SendDataServiceBatchGetPlayerHomeZone};
    gRpcMethodRegistry[DataServiceDeletePlayerDataMessageId] = RpcMethodMeta{
        "DataService", "DeletePlayerData",
        std::make_unique<::data_service::DeletePlayerDataRequest>(),
        std::make_unique<::data_service::DeletePlayerDataResponse>(),
        nullptr, 0, common::base::eNodeType::DataServiceNodeService, data_service::SendDataServiceDeletePlayerData};
    gRpcMethodRegistry[DataServiceCreatePlayerSnapshotMessageId] = RpcMethodMeta{
        "DataService", "CreatePlayerSnapshot",
        std::make_unique<::data_service::CreatePlayerSnapshotRequest>(),
        std::make_unique<::data_service::CreatePlayerSnapshotResponse>(),
        nullptr, 0, common::base::eNodeType::DataServiceNodeService, data_service::SendDataServiceCreatePlayerSnapshot};
    gRpcMethodRegistry[DataServiceListPlayerSnapshotsMessageId] = RpcMethodMeta{
        "DataService", "ListPlayerSnapshots",
        std::make_unique<::data_service::ListPlayerSnapshotsRequest>(),
        std::make_unique<::data_service::ListPlayerSnapshotsResponse>(),
        nullptr, 0, common::base::eNodeType::DataServiceNodeService, data_service::SendDataServiceListPlayerSnapshots};
    gRpcMethodRegistry[DataServiceGetPlayerSnapshotDiffMessageId] = RpcMethodMeta{
        "DataService", "GetPlayerSnapshotDiff",
        std::make_unique<::data_service::GetPlayerSnapshotDiffRequest>(),
        std::make_unique<::data_service::GetPlayerSnapshotDiffResponse>(),
        nullptr, 0, common::base::eNodeType::DataServiceNodeService, data_service::SendDataServiceGetPlayerSnapshotDiff};
    gRpcMethodRegistry[DataServiceRollbackPlayerMessageId] = RpcMethodMeta{
        "DataService", "RollbackPlayer",
        std::make_unique<::data_service::RollbackPlayerRequest>(),
        std::make_unique<::data_service::RollbackPlayerResponse>(),
        nullptr, 0, common::base::eNodeType::DataServiceNodeService, data_service::SendDataServiceRollbackPlayer};
    gRpcMethodRegistry[DataServiceRollbackZoneMessageId] = RpcMethodMeta{
        "DataService", "RollbackZone",
        std::make_unique<::data_service::RollbackZoneRequest>(),
        std::make_unique<::data_service::RollbackZoneResponse>(),
        nullptr, 0, common::base::eNodeType::DataServiceNodeService, data_service::SendDataServiceRollbackZone};
    gRpcMethodRegistry[DataServiceRollbackAllMessageId] = RpcMethodMeta{
        "DataService", "RollbackAll",
        std::make_unique<::data_service::RollbackAllRequest>(),
        std::make_unique<::data_service::RollbackAllResponse>(),
        nullptr, 0, common::base::eNodeType::DataServiceNodeService, data_service::SendDataServiceRollbackAll};
    gRpcMethodRegistry[DataServiceBatchRecallItemsMessageId] = RpcMethodMeta{
        "DataService", "BatchRecallItems",
        std::make_unique<::data_service::BatchRecallItemsRequest>(),
        std::make_unique<::data_service::BatchRecallItemsResponse>(),
        nullptr, 0, common::base::eNodeType::DataServiceNodeService, data_service::SendDataServiceBatchRecallItems};
    gRpcMethodRegistry[DataServiceQueryTransactionLogMessageId] = RpcMethodMeta{
        "DataService", "QueryTransactionLog",
        std::make_unique<::data_service::QueryTransactionLogRequest>(),
        std::make_unique<::data_service::QueryTransactionLogResponse>(),
        nullptr, 0, common::base::eNodeType::DataServiceNodeService, data_service::SendDataServiceQueryTransactionLog};
    gRpcMethodRegistry[DataServiceCreateEventSnapshotMessageId] = RpcMethodMeta{
        "DataService", "CreateEventSnapshot",
        std::make_unique<::data_service::CreateEventSnapshotRequest>(),
        std::make_unique<::data_service::CreateEventSnapshotResponse>(),
        nullptr, 0, common::base::eNodeType::DataServiceNodeService, data_service::SendDataServiceCreateEventSnapshot};

    // --- KV ---
    gRpcMethodRegistry[KVRangeMessageId] = RpcMethodMeta{
        "KV", "Range",
        std::make_unique<::etcdserverpb::RangeRequest>(),
        std::make_unique<::etcdserverpb::RangeResponse>(),
        nullptr, 0, common::base::eNodeType::EtcdNodeService, etcdserverpb::SendKVRange};
    gRpcMethodRegistry[KVPutMessageId] = RpcMethodMeta{
        "KV", "Put",
        std::make_unique<::etcdserverpb::PutRequest>(),
        std::make_unique<::etcdserverpb::PutResponse>(),
        nullptr, 0, common::base::eNodeType::EtcdNodeService, etcdserverpb::SendKVPut};
    gRpcMethodRegistry[KVDeleteRangeMessageId] = RpcMethodMeta{
        "KV", "DeleteRange",
        std::make_unique<::etcdserverpb::DeleteRangeRequest>(),
        std::make_unique<::etcdserverpb::DeleteRangeResponse>(),
        nullptr, 0, common::base::eNodeType::EtcdNodeService, etcdserverpb::SendKVDeleteRange};
    gRpcMethodRegistry[KVTxnMessageId] = RpcMethodMeta{
        "KV", "Txn",
        std::make_unique<::etcdserverpb::TxnRequest>(),
        std::make_unique<::etcdserverpb::TxnResponse>(),
        nullptr, 0, common::base::eNodeType::EtcdNodeService, etcdserverpb::SendKVTxn};
    gRpcMethodRegistry[KVCompactMessageId] = RpcMethodMeta{
        "KV", "Compact",
        std::make_unique<::etcdserverpb::CompactionRequest>(),
        std::make_unique<::etcdserverpb::CompactionResponse>(),
        nullptr, 0, common::base::eNodeType::EtcdNodeService, etcdserverpb::SendKVCompact};

    // --- Watch ---
    gRpcMethodRegistry[WatchWatchMessageId] = RpcMethodMeta{
        "Watch", "Watch",
        std::make_unique<::etcdserverpb::WatchRequest>(),
        std::make_unique<::etcdserverpb::WatchResponse>(),
        nullptr, 0, common::base::eNodeType::EtcdNodeService, etcdserverpb::SendWatchWatch};

    // --- Lease ---
    gRpcMethodRegistry[LeaseLeaseGrantMessageId] = RpcMethodMeta{
        "Lease", "LeaseGrant",
        std::make_unique<::etcdserverpb::LeaseGrantRequest>(),
        std::make_unique<::etcdserverpb::LeaseGrantResponse>(),
        nullptr, 0, common::base::eNodeType::EtcdNodeService, etcdserverpb::SendLeaseLeaseGrant};
    gRpcMethodRegistry[LeaseLeaseRevokeMessageId] = RpcMethodMeta{
        "Lease", "LeaseRevoke",
        std::make_unique<::etcdserverpb::LeaseRevokeRequest>(),
        std::make_unique<::etcdserverpb::LeaseRevokeResponse>(),
        nullptr, 0, common::base::eNodeType::EtcdNodeService, etcdserverpb::SendLeaseLeaseRevoke};
    gRpcMethodRegistry[LeaseLeaseKeepAliveMessageId] = RpcMethodMeta{
        "Lease", "LeaseKeepAlive",
        std::make_unique<::etcdserverpb::LeaseKeepAliveRequest>(),
        std::make_unique<::etcdserverpb::LeaseKeepAliveResponse>(),
        nullptr, 0, common::base::eNodeType::EtcdNodeService, etcdserverpb::SendLeaseLeaseKeepAlive};
    gRpcMethodRegistry[LeaseLeaseTimeToLiveMessageId] = RpcMethodMeta{
        "Lease", "LeaseTimeToLive",
        std::make_unique<::etcdserverpb::LeaseTimeToLiveRequest>(),
        std::make_unique<::etcdserverpb::LeaseTimeToLiveResponse>(),
        nullptr, 0, common::base::eNodeType::EtcdNodeService, etcdserverpb::SendLeaseLeaseTimeToLive};
    gRpcMethodRegistry[LeaseLeaseLeasesMessageId] = RpcMethodMeta{
        "Lease", "LeaseLeases",
        std::make_unique<::etcdserverpb::LeaseLeasesRequest>(),
        std::make_unique<::etcdserverpb::LeaseLeasesResponse>(),
        nullptr, 0, common::base::eNodeType::EtcdNodeService, etcdserverpb::SendLeaseLeaseLeases};

    // --- FriendService ---
    gRpcMethodRegistry[FriendServiceAddFriendMessageId] = RpcMethodMeta{
        "FriendService", "AddFriend",
        std::make_unique<::friendpb::AddFriendRequest>(),
        std::make_unique<::friendpb::AddFriendResponse>(),
        nullptr, 0, common::base::eNodeType::FriendNodeService, friendpb::SendFriendServiceAddFriend};
    gRpcMethodRegistry[FriendServiceAcceptFriendMessageId] = RpcMethodMeta{
        "FriendService", "AcceptFriend",
        std::make_unique<::friendpb::AcceptFriendRequest>(),
        std::make_unique<::friendpb::AcceptFriendResponse>(),
        nullptr, 0, common::base::eNodeType::FriendNodeService, friendpb::SendFriendServiceAcceptFriend};
    gRpcMethodRegistry[FriendServiceRejectFriendMessageId] = RpcMethodMeta{
        "FriendService", "RejectFriend",
        std::make_unique<::friendpb::RejectFriendRequest>(),
        std::make_unique<::friendpb::RejectFriendResponse>(),
        nullptr, 0, common::base::eNodeType::FriendNodeService, friendpb::SendFriendServiceRejectFriend};
    gRpcMethodRegistry[FriendServiceRemoveFriendMessageId] = RpcMethodMeta{
        "FriendService", "RemoveFriend",
        std::make_unique<::friendpb::RemoveFriendRequest>(),
        std::make_unique<::friendpb::RemoveFriendResponse>(),
        nullptr, 0, common::base::eNodeType::FriendNodeService, friendpb::SendFriendServiceRemoveFriend};
    gRpcMethodRegistry[FriendServiceGetFriendListMessageId] = RpcMethodMeta{
        "FriendService", "GetFriendList",
        std::make_unique<::friendpb::GetFriendListRequest>(),
        std::make_unique<::friendpb::GetFriendListResponse>(),
        nullptr, 0, common::base::eNodeType::FriendNodeService, friendpb::SendFriendServiceGetFriendList};
    gRpcMethodRegistry[FriendServiceGetPendingRequestsMessageId] = RpcMethodMeta{
        "FriendService", "GetPendingRequests",
        std::make_unique<::friendpb::GetPendingRequestsRequest>(),
        std::make_unique<::friendpb::GetPendingRequestsResponse>(),
        nullptr, 0, common::base::eNodeType::FriendNodeService, friendpb::SendFriendServiceGetPendingRequests};

    // --- Gate ---
    gRpcMethodRegistry[GatePlayerEnterGameNodeMessageId] = RpcMethodMeta{
        "Gate", "PlayerEnterGameNode",
        std::make_unique<::RegisterGameNodeSessionRequest>(),
        std::make_unique<::RegisterGameNodeSessionResponse>(),
        std::make_unique<GateImpl>(), 0, common::base::eNodeType::GateNodeService};
    gRpcMethodRegistry[GateSendMessageToPlayerMessageId] = RpcMethodMeta{
        "Gate", "SendMessageToPlayer",
        std::make_unique<::NodeRouteMessageRequest>(),
        std::make_unique<::Empty>(),
        std::make_unique<GateImpl>(), 0, common::base::eNodeType::GateNodeService};
    gRpcMethodRegistry[GateRouteNodeMessageMessageId] = RpcMethodMeta{
        "Gate", "RouteNodeMessage",
        std::make_unique<::RouteMessageRequest>(),
        std::make_unique<::RouteMessageResponse>(),
        std::make_unique<GateImpl>(), 0, common::base::eNodeType::GateNodeService};
    gRpcMethodRegistry[GateRoutePlayerMessageMessageId] = RpcMethodMeta{
        "Gate", "RoutePlayerMessage",
        std::make_unique<::RoutePlayerMessageRequest>(),
        std::make_unique<::RoutePlayerMessageResponse>(),
        std::make_unique<GateImpl>(), 0, common::base::eNodeType::GateNodeService};
    gRpcMethodRegistry[GateBroadcastToPlayersMessageId] = RpcMethodMeta{
        "Gate", "BroadcastToPlayers",
        std::make_unique<::BroadcastToPlayersRequest>(),
        std::make_unique<::Empty>(),
        std::make_unique<GateImpl>(), 0, common::base::eNodeType::GateNodeService};
    gRpcMethodRegistry[GateNodeHandshakeMessageId] = RpcMethodMeta{
        "Gate", "NodeHandshake",
        std::make_unique<::NodeHandshakeRequest>(),
        std::make_unique<::NodeHandshakeResponse>(),
        std::make_unique<GateImpl>(), 0, common::base::eNodeType::GateNodeService};
    gRpcMethodRegistry[GateBindSessionToGateMessageId] = RpcMethodMeta{
        "Gate", "BindSessionToGate",
        std::make_unique<::BindSessionToGateRequest>(),
        std::make_unique<::BindSessionToGateResponse>(),
        std::make_unique<GateImpl>(), 0, common::base::eNodeType::GateNodeService};

    // --- GuildService ---
    gRpcMethodRegistry[GuildServiceCreateGuildMessageId] = RpcMethodMeta{
        "GuildService", "CreateGuild",
        std::make_unique<::guildpb::CreateGuildRequest>(),
        std::make_unique<::guildpb::CreateGuildResponse>(),
        nullptr, 0, common::base::eNodeType::GuildNodeService, guildpb::SendGuildServiceCreateGuild};
    gRpcMethodRegistry[GuildServiceGetGuildMessageId] = RpcMethodMeta{
        "GuildService", "GetGuild",
        std::make_unique<::guildpb::GetGuildRequest>(),
        std::make_unique<::guildpb::GetGuildResponse>(),
        nullptr, 0, common::base::eNodeType::GuildNodeService, guildpb::SendGuildServiceGetGuild};
    gRpcMethodRegistry[GuildServiceGetPlayerGuildMessageId] = RpcMethodMeta{
        "GuildService", "GetPlayerGuild",
        std::make_unique<::guildpb::GetPlayerGuildRequest>(),
        std::make_unique<::guildpb::GetPlayerGuildResponse>(),
        nullptr, 0, common::base::eNodeType::GuildNodeService, guildpb::SendGuildServiceGetPlayerGuild};
    gRpcMethodRegistry[GuildServiceJoinGuildMessageId] = RpcMethodMeta{
        "GuildService", "JoinGuild",
        std::make_unique<::guildpb::JoinGuildRequest>(),
        std::make_unique<::guildpb::JoinGuildResponse>(),
        nullptr, 0, common::base::eNodeType::GuildNodeService, guildpb::SendGuildServiceJoinGuild};
    gRpcMethodRegistry[GuildServiceLeaveGuildMessageId] = RpcMethodMeta{
        "GuildService", "LeaveGuild",
        std::make_unique<::guildpb::LeaveGuildRequest>(),
        std::make_unique<::guildpb::LeaveGuildResponse>(),
        nullptr, 0, common::base::eNodeType::GuildNodeService, guildpb::SendGuildServiceLeaveGuild};
    gRpcMethodRegistry[GuildServiceDisbandGuildMessageId] = RpcMethodMeta{
        "GuildService", "DisbandGuild",
        std::make_unique<::guildpb::DisbandGuildRequest>(),
        std::make_unique<::guildpb::DisbandGuildResponse>(),
        nullptr, 0, common::base::eNodeType::GuildNodeService, guildpb::SendGuildServiceDisbandGuild};
    gRpcMethodRegistry[GuildServiceSetAnnouncementMessageId] = RpcMethodMeta{
        "GuildService", "SetAnnouncement",
        std::make_unique<::guildpb::SetAnnouncementRequest>(),
        std::make_unique<::guildpb::SetAnnouncementResponse>(),
        nullptr, 0, common::base::eNodeType::GuildNodeService, guildpb::SendGuildServiceSetAnnouncement};
    gRpcMethodRegistry[GuildServiceUpdateGuildScoreMessageId] = RpcMethodMeta{
        "GuildService", "UpdateGuildScore",
        std::make_unique<::guildpb::UpdateGuildScoreRequest>(),
        std::make_unique<::guildpb::UpdateGuildScoreResponse>(),
        nullptr, 0, common::base::eNodeType::GuildNodeService, guildpb::SendGuildServiceUpdateGuildScore};
    gRpcMethodRegistry[GuildServiceGetGuildRankMessageId] = RpcMethodMeta{
        "GuildService", "GetGuildRank",
        std::make_unique<::guildpb::GetGuildRankRequest>(),
        std::make_unique<::guildpb::GetGuildRankResponse>(),
        nullptr, 0, common::base::eNodeType::GuildNodeService, guildpb::SendGuildServiceGetGuildRank};
    gRpcMethodRegistry[GuildServiceGetGuildRankByGuildMessageId] = RpcMethodMeta{
        "GuildService", "GetGuildRankByGuild",
        std::make_unique<::guildpb::GetGuildRankByGuildRequest>(),
        std::make_unique<::guildpb::GetGuildRankByGuildResponse>(),
        nullptr, 0, common::base::eNodeType::GuildNodeService, guildpb::SendGuildServiceGetGuildRankByGuild};

    // --- ClientPlayerLogin ---
    gRpcMethodRegistry[ClientPlayerLoginLoginMessageId] = RpcMethodMeta{
        "ClientPlayerLogin", "Login",
        std::make_unique<::loginpb::LoginRequest>(),
        std::make_unique<::loginpb::LoginResponse>(),
        nullptr, 0, common::base::eNodeType::LoginNodeService, loginpb::SendClientPlayerLoginLogin};
    gRpcMethodRegistry[ClientPlayerLoginCreatePlayerMessageId] = RpcMethodMeta{
        "ClientPlayerLogin", "CreatePlayer",
        std::make_unique<::loginpb::CreatePlayerRequest>(),
        std::make_unique<::loginpb::CreatePlayerResponse>(),
        nullptr, 0, common::base::eNodeType::LoginNodeService, loginpb::SendClientPlayerLoginCreatePlayer};
    gRpcMethodRegistry[ClientPlayerLoginEnterGameMessageId] = RpcMethodMeta{
        "ClientPlayerLogin", "EnterGame",
        std::make_unique<::loginpb::EnterGameRequest>(),
        std::make_unique<::loginpb::EnterGameResponse>(),
        nullptr, 0, common::base::eNodeType::LoginNodeService, loginpb::SendClientPlayerLoginEnterGame};
    gRpcMethodRegistry[ClientPlayerLoginLeaveGameMessageId] = RpcMethodMeta{
        "ClientPlayerLogin", "LeaveGame",
        std::make_unique<::loginpb::LeaveGameRequest>(),
        std::make_unique<::loginpb::LoginEmptyResponse>(),
        nullptr, 0, common::base::eNodeType::LoginNodeService, loginpb::SendClientPlayerLoginLeaveGame};
    gRpcMethodRegistry[ClientPlayerLoginDisconnectMessageId] = RpcMethodMeta{
        "ClientPlayerLogin", "Disconnect",
        std::make_unique<::loginpb::LoginNodeDisconnectRequest>(),
        std::make_unique<::loginpb::LoginEmptyResponse>(),
        nullptr, 0, common::base::eNodeType::LoginNodeService, loginpb::SendClientPlayerLoginDisconnect};

    // --- LoginPreGate ---
    gRpcMethodRegistry[LoginPreGateAssignGateMessageId] = RpcMethodMeta{
        "LoginPreGate", "AssignGate",
        std::make_unique<::loginpb::AssignGateRequest>(),
        std::make_unique<::loginpb::AssignGateResponse>(),
        nullptr, 0, common::base::eNodeType::LoginNodeService, loginpb::SendLoginPreGateAssignGate};

    // --- LoginAdmin ---
    gRpcMethodRegistry[LoginAdminRemovePlayersFromAccountsMessageId] = RpcMethodMeta{
        "LoginAdmin", "RemovePlayersFromAccounts",
        std::make_unique<::loginpb::RemovePlayersFromAccountsRequest>(),
        std::make_unique<::loginpb::RemovePlayersFromAccountsResponse>(),
        nullptr, 0, common::base::eNodeType::LoginNodeService, loginpb::SendLoginAdminRemovePlayersFromAccounts};

    // --- SceneClientPlayerCommon ---
    gRpcMethodRegistry[SceneClientPlayerCommonSendTipToClientMessageId] = RpcMethodMeta{
        "SceneClientPlayerCommon", "SendTipToClient",
        std::make_unique<::TipInfoMessage>(),
        std::make_unique<::Empty>(),
        std::make_unique<SceneClientPlayerCommonImpl>(), 0, common::base::eNodeType::SceneNodeService};
    gRpcMethodRegistry[SceneClientPlayerCommonKickPlayerMessageId] = RpcMethodMeta{
        "SceneClientPlayerCommon", "KickPlayer",
        std::make_unique<::GameKickPlayerRequest>(),
        std::make_unique<::Empty>(),
        std::make_unique<SceneClientPlayerCommonImpl>(), 0, common::base::eNodeType::SceneNodeService};

    // --- ScenePlayer ---
    gRpcMethodRegistry[ScenePlayerGateLoginNotifyMessageId] = RpcMethodMeta{
        "ScenePlayer", "GateLoginNotify",
        std::make_unique<::GateLoginNotifyRequest>(),
        std::make_unique<::google::protobuf::Empty>(),
        std::make_unique<ScenePlayerImpl>(), 0, common::base::eNodeType::SceneNodeService};
    gRpcMethodRegistry[ScenePlayerExitGameMessageId] = RpcMethodMeta{
        "ScenePlayer", "ExitGame",
        std::make_unique<::GameNodeExitGameRequest>(),
        std::make_unique<::google::protobuf::Empty>(),
        std::make_unique<ScenePlayerImpl>(), 0, common::base::eNodeType::SceneNodeService};

    // --- SceneScenePlayer ---
    gRpcMethodRegistry[SceneScenePlayerEnterSceneMessageId] = RpcMethodMeta{
        "SceneScenePlayer", "EnterScene",
        std::make_unique<::GsEnterSceneRequest>(),
        std::make_unique<::google::protobuf::Empty>(),
        std::make_unique<SceneScenePlayerImpl>(), 0, common::base::eNodeType::SceneNodeService};
    gRpcMethodRegistry[SceneScenePlayerLeaveSceneMessageId] = RpcMethodMeta{
        "SceneScenePlayer", "LeaveScene",
        std::make_unique<::GsLeaveSceneRequest>(),
        std::make_unique<::google::protobuf::Empty>(),
        std::make_unique<SceneScenePlayerImpl>(), 0, common::base::eNodeType::SceneNodeService};
    gRpcMethodRegistry[SceneScenePlayerEnterSceneS2CMessageId] = RpcMethodMeta{
        "SceneScenePlayer", "EnterSceneS2C",
        std::make_unique<::EnterSceneS2CRequest>(),
        std::make_unique<::EnterSceneS2CResponse>(),
        std::make_unique<SceneScenePlayerImpl>(), 0, common::base::eNodeType::SceneNodeService};

    // --- SceneScene ---
    gRpcMethodRegistry[SceneSceneTestMessageId] = RpcMethodMeta{
        "SceneScene", "Test",
        std::make_unique<::GameSceneTest>(),
        std::make_unique<::Empty>(),
        std::make_unique<SceneSceneImpl>(), 0, common::base::eNodeType::SceneNodeService};

    // --- SceneCurrencyClientPlayer ---
    gRpcMethodRegistry[SceneCurrencyClientPlayerGmAddCurrencyMessageId] = RpcMethodMeta{
        "SceneCurrencyClientPlayer", "GmAddCurrency",
        std::make_unique<::GmAddCurrencyRequest>(),
        std::make_unique<::GmAddCurrencyResponse>(),
        std::make_unique<SceneCurrencyClientPlayerImpl>(), 0, common::base::eNodeType::SceneNodeService};
    gRpcMethodRegistry[SceneCurrencyClientPlayerGmDeductCurrencyMessageId] = RpcMethodMeta{
        "SceneCurrencyClientPlayer", "GmDeductCurrency",
        std::make_unique<::GmDeductCurrencyRequest>(),
        std::make_unique<::GmDeductCurrencyResponse>(),
        std::make_unique<SceneCurrencyClientPlayerImpl>(), 0, common::base::eNodeType::SceneNodeService};
    gRpcMethodRegistry[SceneCurrencyClientPlayerGetCurrencyListMessageId] = RpcMethodMeta{
        "SceneCurrencyClientPlayer", "GetCurrencyList",
        std::make_unique<::GetCurrencyListRequest>(),
        std::make_unique<::GetCurrencyListResponse>(),
        std::make_unique<SceneCurrencyClientPlayerImpl>(), 0, common::base::eNodeType::SceneNodeService};
    gRpcMethodRegistry[SceneCurrencyClientPlayerGmBlockCurrencyMessageId] = RpcMethodMeta{
        "SceneCurrencyClientPlayer", "GmBlockCurrency",
        std::make_unique<::GmBlockCurrencyRequest>(),
        std::make_unique<::GmBlockCurrencyResponse>(),
        std::make_unique<SceneCurrencyClientPlayerImpl>(), 0, common::base::eNodeType::SceneNodeService};
    gRpcMethodRegistry[SceneCurrencyClientPlayerGmUnblockCurrencyMessageId] = RpcMethodMeta{
        "SceneCurrencyClientPlayer", "GmUnblockCurrency",
        std::make_unique<::GmUnblockCurrencyRequest>(),
        std::make_unique<::GmUnblockCurrencyResponse>(),
        std::make_unique<SceneCurrencyClientPlayerImpl>(), 0, common::base::eNodeType::SceneNodeService};

    // --- SceneRollbackClientPlayer ---
    gRpcMethodRegistry[SceneRollbackClientPlayerGmAttachDebtMessageId] = RpcMethodMeta{
        "SceneRollbackClientPlayer", "GmAttachDebt",
        std::make_unique<::GmAttachDebtRequest>(),
        std::make_unique<::GmAttachDebtResponse>(),
        std::make_unique<SceneRollbackClientPlayerImpl>(), 0, common::base::eNodeType::SceneNodeService};
    gRpcMethodRegistry[SceneRollbackClientPlayerGmWaiveDebtMessageId] = RpcMethodMeta{
        "SceneRollbackClientPlayer", "GmWaiveDebt",
        std::make_unique<::GmWaiveDebtRequest>(),
        std::make_unique<::GmWaiveDebtResponse>(),
        std::make_unique<SceneRollbackClientPlayerImpl>(), 0, common::base::eNodeType::SceneNodeService};
    gRpcMethodRegistry[SceneRollbackClientPlayerGmAdjustDebtMessageId] = RpcMethodMeta{
        "SceneRollbackClientPlayer", "GmAdjustDebt",
        std::make_unique<::GmAdjustDebtRequest>(),
        std::make_unique<::GmAdjustDebtResponse>(),
        std::make_unique<SceneRollbackClientPlayerImpl>(), 0, common::base::eNodeType::SceneNodeService};
    gRpcMethodRegistry[SceneRollbackClientPlayerGmFreezeDebtMessageId] = RpcMethodMeta{
        "SceneRollbackClientPlayer", "GmFreezeDebt",
        std::make_unique<::GmFreezeDebtRequest>(),
        std::make_unique<::GmFreezeDebtResponse>(),
        std::make_unique<SceneRollbackClientPlayerImpl>(), 0, common::base::eNodeType::SceneNodeService};
    gRpcMethodRegistry[SceneRollbackClientPlayerGmQueryDebtMessageId] = RpcMethodMeta{
        "SceneRollbackClientPlayer", "GmQueryDebt",
        std::make_unique<::GmQueryDebtRequest>(),
        std::make_unique<::GmQueryDebtResponse>(),
        std::make_unique<SceneRollbackClientPlayerImpl>(), 0, common::base::eNodeType::SceneNodeService};
    gRpcMethodRegistry[SceneRollbackClientPlayerGmCreateSnapshotMessageId] = RpcMethodMeta{
        "SceneRollbackClientPlayer", "GmCreateSnapshot",
        std::make_unique<::GmCreateSnapshotRequest>(),
        std::make_unique<::GmCreateSnapshotResponse>(),
        std::make_unique<SceneRollbackClientPlayerImpl>(), 0, common::base::eNodeType::SceneNodeService};
    gRpcMethodRegistry[SceneRollbackClientPlayerGmListSnapshotsMessageId] = RpcMethodMeta{
        "SceneRollbackClientPlayer", "GmListSnapshots",
        std::make_unique<::GmListSnapshotsRequest>(),
        std::make_unique<::GmListSnapshotsResponse>(),
        std::make_unique<SceneRollbackClientPlayerImpl>(), 0, common::base::eNodeType::SceneNodeService};
    gRpcMethodRegistry[SceneRollbackClientPlayerGmPreviewRollbackMessageId] = RpcMethodMeta{
        "SceneRollbackClientPlayer", "GmPreviewRollback",
        std::make_unique<::GmPreviewRollbackRequest>(),
        std::make_unique<::GmPreviewRollbackResponse>(),
        std::make_unique<SceneRollbackClientPlayerImpl>(), 0, common::base::eNodeType::SceneNodeService};
    gRpcMethodRegistry[SceneRollbackClientPlayerGmExecuteRollbackMessageId] = RpcMethodMeta{
        "SceneRollbackClientPlayer", "GmExecuteRollback",
        std::make_unique<::GmExecuteRollbackRequest>(),
        std::make_unique<::GmExecuteRollbackResponse>(),
        std::make_unique<SceneRollbackClientPlayerImpl>(), 0, common::base::eNodeType::SceneNodeService};
    gRpcMethodRegistry[SceneRollbackClientPlayerGmQueryTransactionLogMessageId] = RpcMethodMeta{
        "SceneRollbackClientPlayer", "GmQueryTransactionLog",
        std::make_unique<::GmQueryTransactionLogRequest>(),
        std::make_unique<::GmQueryTransactionLogResponse>(),
        std::make_unique<SceneRollbackClientPlayerImpl>(), 0, common::base::eNodeType::SceneNodeService};
    gRpcMethodRegistry[SceneRollbackClientPlayerGmTraceItemMessageId] = RpcMethodMeta{
        "SceneRollbackClientPlayer", "GmTraceItem",
        std::make_unique<::GmTraceItemRequest>(),
        std::make_unique<::GmTraceItemResponse>(),
        std::make_unique<SceneRollbackClientPlayerImpl>(), 0, common::base::eNodeType::SceneNodeService};
    gRpcMethodRegistry[SceneRollbackClientPlayerGmClawbackItemMessageId] = RpcMethodMeta{
        "SceneRollbackClientPlayer", "GmClawbackItem",
        std::make_unique<::GmClawbackItemRequest>(),
        std::make_unique<::GmClawbackItemResponse>(),
        std::make_unique<SceneRollbackClientPlayerImpl>(), 0, common::base::eNodeType::SceneNodeService};

    // --- SceneSceneClientPlayer ---
    gRpcMethodRegistry[SceneSceneClientPlayerEnterSceneMessageId] = RpcMethodMeta{
        "SceneSceneClientPlayer", "EnterScene",
        std::make_unique<::EnterSceneC2SRequest>(),
        std::make_unique<::EnterSceneC2SResponse>(),
        std::make_unique<SceneSceneClientPlayerImpl>(), 0, common::base::eNodeType::SceneNodeService};
    gRpcMethodRegistry[SceneSceneClientPlayerNotifyEnterSceneMessageId] = RpcMethodMeta{
        "SceneSceneClientPlayer", "NotifyEnterScene",
        std::make_unique<::EnterSceneS2C>(),
        std::make_unique<::Empty>(),
        std::make_unique<SceneSceneClientPlayerImpl>(), 0, common::base::eNodeType::SceneNodeService};
    gRpcMethodRegistry[SceneSceneClientPlayerSceneInfoC2SMessageId] = RpcMethodMeta{
        "SceneSceneClientPlayer", "SceneInfoC2S",
        std::make_unique<::SceneInfoRequest>(),
        std::make_unique<::Empty>(),
        std::make_unique<SceneSceneClientPlayerImpl>(), 0, common::base::eNodeType::SceneNodeService};
    gRpcMethodRegistry[SceneSceneClientPlayerNotifySceneInfoMessageId] = RpcMethodMeta{
        "SceneSceneClientPlayer", "NotifySceneInfo",
        std::make_unique<::SceneInfoS2C>(),
        std::make_unique<::Empty>(),
        std::make_unique<SceneSceneClientPlayerImpl>(), 0, common::base::eNodeType::SceneNodeService};
    gRpcMethodRegistry[SceneSceneClientPlayerNotifyActorCreateMessageId] = RpcMethodMeta{
        "SceneSceneClientPlayer", "NotifyActorCreate",
        std::make_unique<::ActorCreateS2C>(),
        std::make_unique<::Empty>(),
        std::make_unique<SceneSceneClientPlayerImpl>(), 0, common::base::eNodeType::SceneNodeService};
    gRpcMethodRegistry[SceneSceneClientPlayerNotifyActorDestroyMessageId] = RpcMethodMeta{
        "SceneSceneClientPlayer", "NotifyActorDestroy",
        std::make_unique<::ActorDestroyS2C>(),
        std::make_unique<::Empty>(),
        std::make_unique<SceneSceneClientPlayerImpl>(), 0, common::base::eNodeType::SceneNodeService};
    gRpcMethodRegistry[SceneSceneClientPlayerNotifyActorListCreateMessageId] = RpcMethodMeta{
        "SceneSceneClientPlayer", "NotifyActorListCreate",
        std::make_unique<::ActorListCreateS2C>(),
        std::make_unique<::Empty>(),
        std::make_unique<SceneSceneClientPlayerImpl>(), 0, common::base::eNodeType::SceneNodeService};
    gRpcMethodRegistry[SceneSceneClientPlayerNotifyActorListDestroyMessageId] = RpcMethodMeta{
        "SceneSceneClientPlayer", "NotifyActorListDestroy",
        std::make_unique<::ActorListDestroyS2C>(),
        std::make_unique<::Empty>(),
        std::make_unique<SceneSceneClientPlayerImpl>(), 0, common::base::eNodeType::SceneNodeService};

    // --- SceneSkillClientPlayer ---
    gRpcMethodRegistry[SceneSkillClientPlayerReleaseSkillMessageId] = RpcMethodMeta{
        "SceneSkillClientPlayer", "ReleaseSkill",
        std::make_unique<::ReleaseSkillRequest>(),
        std::make_unique<::ReleaseSkillResponse>(),
        std::make_unique<SceneSkillClientPlayerImpl>(), 0, common::base::eNodeType::SceneNodeService};
    gRpcMethodRegistry[SceneSkillClientPlayerNotifySkillUsedMessageId] = RpcMethodMeta{
        "SceneSkillClientPlayer", "NotifySkillUsed",
        std::make_unique<::SkillUsedS2C>(),
        std::make_unique<::Empty>(),
        std::make_unique<SceneSkillClientPlayerImpl>(), 0, common::base::eNodeType::SceneNodeService};
    gRpcMethodRegistry[SceneSkillClientPlayerNotifySkillInterruptedMessageId] = RpcMethodMeta{
        "SceneSkillClientPlayer", "NotifySkillInterrupted",
        std::make_unique<::SkillInterruptedS2C>(),
        std::make_unique<::Empty>(),
        std::make_unique<SceneSkillClientPlayerImpl>(), 0, common::base::eNodeType::SceneNodeService};
    gRpcMethodRegistry[SceneSkillClientPlayerGetSkillListMessageId] = RpcMethodMeta{
        "SceneSkillClientPlayer", "GetSkillList",
        std::make_unique<::GetSkillListRequest>(),
        std::make_unique<::GetSkillListResponse>(),
        std::make_unique<SceneSkillClientPlayerImpl>(), 0, common::base::eNodeType::SceneNodeService};

    // --- ScenePlayerSync ---
    gRpcMethodRegistry[ScenePlayerSyncSyncBaseAttributeMessageId] = RpcMethodMeta{
        "ScenePlayerSync", "SyncBaseAttribute",
        std::make_unique<::ActorBaseAttributesS2C>(),
        std::make_unique<::Empty>(),
        std::make_unique<ScenePlayerSyncImpl>(), 0, common::base::eNodeType::SceneNodeService};
    gRpcMethodRegistry[ScenePlayerSyncSyncAttribute2FramesMessageId] = RpcMethodMeta{
        "ScenePlayerSync", "SyncAttribute2Frames",
        std::make_unique<::AttributeDelta2FramesS2C>(),
        std::make_unique<::Empty>(),
        std::make_unique<ScenePlayerSyncImpl>(), 0, common::base::eNodeType::SceneNodeService};
    gRpcMethodRegistry[ScenePlayerSyncSyncAttribute5FramesMessageId] = RpcMethodMeta{
        "ScenePlayerSync", "SyncAttribute5Frames",
        std::make_unique<::AttributeDelta5FramesS2C>(),
        std::make_unique<::Empty>(),
        std::make_unique<ScenePlayerSyncImpl>(), 0, common::base::eNodeType::SceneNodeService};
    gRpcMethodRegistry[ScenePlayerSyncSyncAttribute10FramesMessageId] = RpcMethodMeta{
        "ScenePlayerSync", "SyncAttribute10Frames",
        std::make_unique<::AttributeDelta10FramesS2C>(),
        std::make_unique<::Empty>(),
        std::make_unique<ScenePlayerSyncImpl>(), 0, common::base::eNodeType::SceneNodeService};
    gRpcMethodRegistry[ScenePlayerSyncSyncAttribute30FramesMessageId] = RpcMethodMeta{
        "ScenePlayerSync", "SyncAttribute30Frames",
        std::make_unique<::AttributeDelta30FramesS2C>(),
        std::make_unique<::Empty>(),
        std::make_unique<ScenePlayerSyncImpl>(), 0, common::base::eNodeType::SceneNodeService};
    gRpcMethodRegistry[ScenePlayerSyncSyncAttribute60FramesMessageId] = RpcMethodMeta{
        "ScenePlayerSync", "SyncAttribute60Frames",
        std::make_unique<::AttributeDelta60FramesS2C>(),
        std::make_unique<::Empty>(),
        std::make_unique<ScenePlayerSyncImpl>(), 0, common::base::eNodeType::SceneNodeService};

    // --- Scene ---
    gRpcMethodRegistry[ScenePlayerEnterGameNodeMessageId] = RpcMethodMeta{
        "Scene", "PlayerEnterGameNode",
        std::make_unique<::PlayerEnterGameNodeRequest>(),
        std::make_unique<::Empty>(),
        std::make_unique<SceneImpl>(), 0, common::base::eNodeType::SceneNodeService};
    gRpcMethodRegistry[SceneSendMessageToPlayerMessageId] = RpcMethodMeta{
        "Scene", "SendMessageToPlayer",
        std::make_unique<::NodeRouteMessageRequest>(),
        std::make_unique<::NodeRouteMessageResponse>(),
        std::make_unique<SceneImpl>(), 0, common::base::eNodeType::SceneNodeService};
    gRpcMethodRegistry[SceneProcessClientPlayerMessageMessageId] = RpcMethodMeta{
        "Scene", "ProcessClientPlayerMessage",
        std::make_unique<::ProcessClientPlayerMessageRequest>(),
        std::make_unique<::ProcessClientPlayerMessageResponse>(),
        std::make_unique<SceneImpl>(), 0, common::base::eNodeType::SceneNodeService};
    gRpcMethodRegistry[SceneCentreSendToPlayerViaGameNodeMessageId] = RpcMethodMeta{
        "Scene", "CentreSendToPlayerViaGameNode",
        std::make_unique<::NodeRouteMessageRequest>(),
        std::make_unique<::Empty>(),
        std::make_unique<SceneImpl>(), 0, common::base::eNodeType::SceneNodeService};
    gRpcMethodRegistry[SceneInvokePlayerServiceMessageId] = RpcMethodMeta{
        "Scene", "InvokePlayerService",
        std::make_unique<::NodeRouteMessageRequest>(),
        std::make_unique<::NodeRouteMessageResponse>(),
        std::make_unique<SceneImpl>(), 0, common::base::eNodeType::SceneNodeService};
    gRpcMethodRegistry[SceneRouteNodeStringMsgMessageId] = RpcMethodMeta{
        "Scene", "RouteNodeStringMsg",
        std::make_unique<::RouteMessageRequest>(),
        std::make_unique<::RouteMessageResponse>(),
        std::make_unique<SceneImpl>(), 0, common::base::eNodeType::SceneNodeService};
    gRpcMethodRegistry[SceneRoutePlayerStringMsgMessageId] = RpcMethodMeta{
        "Scene", "RoutePlayerStringMsg",
        std::make_unique<::RoutePlayerMessageRequest>(),
        std::make_unique<::RoutePlayerMessageResponse>(),
        std::make_unique<SceneImpl>(), 0, common::base::eNodeType::SceneNodeService};
    gRpcMethodRegistry[SceneUpdateSessionDetailMessageId] = RpcMethodMeta{
        "Scene", "UpdateSessionDetail",
        std::make_unique<::RegisterPlayerSessionRequest>(),
        std::make_unique<::Empty>(),
        std::make_unique<SceneImpl>(), 0, common::base::eNodeType::SceneNodeService};
    gRpcMethodRegistry[SceneEnterSceneMessageId] = RpcMethodMeta{
        "Scene", "EnterScene",
        std::make_unique<::Centre2GsEnterSceneRequest>(),
        std::make_unique<::Empty>(),
        std::make_unique<SceneImpl>(), 0, common::base::eNodeType::SceneNodeService};
    gRpcMethodRegistry[SceneCreateSceneMessageId] = RpcMethodMeta{
        "Scene", "CreateScene",
        std::make_unique<::CreateSceneRequest>(),
        std::make_unique<::CreateSceneResponse>(),
        std::make_unique<SceneImpl>(), 0, common::base::eNodeType::SceneNodeService};
    gRpcMethodRegistry[SceneNodeHandshakeMessageId] = RpcMethodMeta{
        "Scene", "NodeHandshake",
        std::make_unique<::NodeHandshakeRequest>(),
        std::make_unique<::NodeHandshakeResponse>(),
        std::make_unique<SceneImpl>(), 0, common::base::eNodeType::SceneNodeService};

    // --- SceneManager ---
    gRpcMethodRegistry[SceneManagerCreateSceneMessageId] = RpcMethodMeta{
        "SceneManager", "CreateScene",
        std::make_unique<::scene_manager::CreateSceneRequest>(),
        std::make_unique<::scene_manager::CreateSceneResponse>(),
        nullptr, 0, common::base::eNodeType::SceneManagerNodeService, scene_manager::SendSceneManagerCreateScene};
    gRpcMethodRegistry[SceneManagerDestroySceneMessageId] = RpcMethodMeta{
        "SceneManager", "DestroyScene",
        std::make_unique<::scene_manager::DestroySceneRequest>(),
        std::make_unique<::Empty>(),
        nullptr, 0, common::base::eNodeType::SceneManagerNodeService, scene_manager::SendSceneManagerDestroyScene};
    gRpcMethodRegistry[SceneManagerEnterSceneMessageId] = RpcMethodMeta{
        "SceneManager", "EnterScene",
        std::make_unique<::scene_manager::EnterSceneRequest>(),
        std::make_unique<::scene_manager::EnterSceneResponse>(),
        nullptr, 0, common::base::eNodeType::SceneManagerNodeService, scene_manager::SendSceneManagerEnterScene};
    gRpcMethodRegistry[SceneManagerLeaveSceneMessageId] = RpcMethodMeta{
        "SceneManager", "LeaveScene",
        std::make_unique<::scene_manager::LeaveSceneRequest>(),
        std::make_unique<::Empty>(),
        nullptr, 0, common::base::eNodeType::SceneManagerNodeService, scene_manager::SendSceneManagerLeaveScene};

    // --- SlgBattle ---
    gRpcMethodRegistry[SlgBattleSimulateBattleMessageId] = RpcMethodMeta{
        "SlgBattle", "SimulateBattle",
        std::make_unique<::SimulateBattleRequest>(),
        std::make_unique<::SimulateBattleResponse>(),
        std::make_unique<SlgBattleImpl>(), 0, common::base::eNodeType::SlgBattleNodeService};
    gRpcMethodRegistry[SlgBattleQueryBattleReportMessageId] = RpcMethodMeta{
        "SlgBattle", "QueryBattleReport",
        std::make_unique<::QueryBattleReportRequest>(),
        std::make_unique<::QueryBattleReportResponse>(),
        std::make_unique<SlgBattleImpl>(), 0, common::base::eNodeType::SlgBattleNodeService};

    // --- SlgMap ---
    gRpcMethodRegistry[SlgMapStartMarchMessageId] = RpcMethodMeta{
        "SlgMap", "StartMarch",
        std::make_unique<::StartMarchRequest>(),
        std::make_unique<::StartMarchResponse>(),
        std::make_unique<SlgMapImpl>(), 0, common::base::eNodeType::SlgMapNodeService};
    gRpcMethodRegistry[SlgMapCancelMarchMessageId] = RpcMethodMeta{
        "SlgMap", "CancelMarch",
        std::make_unique<::CancelMarchRequest>(),
        std::make_unique<::Empty>(),
        std::make_unique<SlgMapImpl>(), 0, common::base::eNodeType::SlgMapNodeService};
    gRpcMethodRegistry[SlgMapQueryViewportMessageId] = RpcMethodMeta{
        "SlgMap", "QueryViewport",
        std::make_unique<::ViewportRequest>(),
        std::make_unique<::ViewportResponse>(),
        std::make_unique<SlgMapImpl>(), 0, common::base::eNodeType::SlgMapNodeService};
    gRpcMethodRegistry[SlgMapBuildMessageId] = RpcMethodMeta{
        "SlgMap", "Build",
        std::make_unique<::BuildRequest>(),
        std::make_unique<::BuildResponse>(),
        std::make_unique<SlgMapImpl>(), 0, common::base::eNodeType::SlgMapNodeService};
    gRpcMethodRegistry[SlgMapUpgradeBuildingMessageId] = RpcMethodMeta{
        "SlgMap", "UpgradeBuilding",
        std::make_unique<::UpgradeBuildingRequest>(),
        std::make_unique<::UpgradeBuildingResponse>(),
        std::make_unique<SlgMapImpl>(), 0, common::base::eNodeType::SlgMapNodeService};
    gRpcMethodRegistry[SlgMapNodeHandshakeMessageId] = RpcMethodMeta{
        "SlgMap", "NodeHandshake",
        std::make_unique<::NodeHandshakeRequest>(),
        std::make_unique<::NodeHandshakeResponse>(),
        std::make_unique<SlgMapImpl>(), 0, common::base::eNodeType::SlgMapNodeService};

    // --- TurnBasedBattle ---
    gRpcMethodRegistry[TurnBasedBattleStartBattleMessageId] = RpcMethodMeta{
        "TurnBasedBattle", "StartBattle",
        std::make_unique<::StartBattleRequest>(),
        std::make_unique<::StartBattleResponse>(),
        std::make_unique<TurnBasedBattleImpl>(), 0, common::base::eNodeType::TurnBasedBattleNodeService};
    gRpcMethodRegistry[TurnBasedBattleExecuteActionMessageId] = RpcMethodMeta{
        "TurnBasedBattle", "ExecuteAction",
        std::make_unique<::ExecuteActionRequest>(),
        std::make_unique<::ExecuteActionResponse>(),
        std::make_unique<TurnBasedBattleImpl>(), 0, common::base::eNodeType::TurnBasedBattleNodeService};
    gRpcMethodRegistry[TurnBasedBattleQueryBattleMessageId] = RpcMethodMeta{
        "TurnBasedBattle", "QueryBattle",
        std::make_unique<::QueryBattleRequest>(),
        std::make_unique<::QueryBattleResponse>(),
        std::make_unique<TurnBasedBattleImpl>(), 0, common::base::eNodeType::TurnBasedBattleNodeService};
    gRpcMethodRegistry[TurnBasedBattleNodeHandshakeMessageId] = RpcMethodMeta{
        "TurnBasedBattle", "NodeHandshake",
        std::make_unique<::NodeHandshakeRequest>(),
        std::make_unique<::NodeHandshakeResponse>(),
        std::make_unique<TurnBasedBattleImpl>(), 0, common::base::eNodeType::TurnBasedBattleNodeService};

    // --- TurnBasedDungeon ---
    gRpcMethodRegistry[TurnBasedDungeonEnterDungeonMessageId] = RpcMethodMeta{
        "TurnBasedDungeon", "EnterDungeon",
        std::make_unique<::EnterDungeonRequest>(),
        std::make_unique<::EnterDungeonResponse>(),
        std::make_unique<TurnBasedDungeonImpl>(), 0, common::base::eNodeType::TurnBasedDungeonNodeService};
    gRpcMethodRegistry[TurnBasedDungeonMoveInDungeonMessageId] = RpcMethodMeta{
        "TurnBasedDungeon", "MoveInDungeon",
        std::make_unique<::MoveInDungeonRequest>(),
        std::make_unique<::MoveInDungeonResponse>(),
        std::make_unique<TurnBasedDungeonImpl>(), 0, common::base::eNodeType::TurnBasedDungeonNodeService};
    gRpcMethodRegistry[TurnBasedDungeonNextFloorMessageId] = RpcMethodMeta{
        "TurnBasedDungeon", "NextFloor",
        std::make_unique<::NextFloorRequest>(),
        std::make_unique<::NextFloorResponse>(),
        std::make_unique<TurnBasedDungeonImpl>(), 0, common::base::eNodeType::TurnBasedDungeonNodeService};
    gRpcMethodRegistry[TurnBasedDungeonQueryDungeonMessageId] = RpcMethodMeta{
        "TurnBasedDungeon", "QueryDungeon",
        std::make_unique<::QueryDungeonRequest>(),
        std::make_unique<::QueryDungeonResponse>(),
        std::make_unique<TurnBasedDungeonImpl>(), 0, common::base::eNodeType::TurnBasedDungeonNodeService};
    gRpcMethodRegistry[TurnBasedDungeonLeaveDungeonMessageId] = RpcMethodMeta{
        "TurnBasedDungeon", "LeaveDungeon",
        std::make_unique<::LeaveDungeonRequest>(),
        std::make_unique<::LeaveDungeonResponse>(),
        std::make_unique<TurnBasedDungeonImpl>(), 0, common::base::eNodeType::TurnBasedDungeonNodeService};
    gRpcMethodRegistry[TurnBasedDungeonNodeHandshakeMessageId] = RpcMethodMeta{
        "TurnBasedDungeon", "NodeHandshake",
        std::make_unique<::NodeHandshakeRequest>(),
        std::make_unique<::NodeHandshakeResponse>(),
        std::make_unique<TurnBasedDungeonImpl>(), 0, common::base::eNodeType::TurnBasedDungeonNodeService};
}

bool IsClientMessageId(uint32_t messageId)
{
	switch (messageId) {
	case ClientPlayerChatSendChatMessageId:
	case ClientPlayerChatPullChatHistoryMessageId:
	case ClientPlayerLoginLoginMessageId:
	case ClientPlayerLoginCreatePlayerMessageId:
	case ClientPlayerLoginEnterGameMessageId:
	case ClientPlayerLoginLeaveGameMessageId:
	case ClientPlayerLoginDisconnectMessageId:
	case SceneClientPlayerCommonSendTipToClientMessageId:
	case SceneClientPlayerCommonKickPlayerMessageId:
	case SceneSceneClientPlayerEnterSceneMessageId:
	case SceneSceneClientPlayerNotifyEnterSceneMessageId:
	case SceneSceneClientPlayerSceneInfoC2SMessageId:
	case SceneSceneClientPlayerNotifySceneInfoMessageId:
	case SceneSceneClientPlayerNotifyActorCreateMessageId:
	case SceneSceneClientPlayerNotifyActorDestroyMessageId:
	case SceneSceneClientPlayerNotifyActorListCreateMessageId:
	case SceneSceneClientPlayerNotifyActorListDestroyMessageId:
	case SceneSkillClientPlayerReleaseSkillMessageId:
	case SceneSkillClientPlayerNotifySkillUsedMessageId:
	case SceneSkillClientPlayerNotifySkillInterruptedMessageId:
	case SceneSkillClientPlayerGetSkillListMessageId:
		return true;
	default:
		return false;
	}
}

bool IsValidEventId(uint32_t eventId)
{
	return eventId < kMaxEventCount;
}

bool DispatchProtoEvent(uint32_t eventId, const std::string& payload)
{
	switch (eventId) {
	case AcceptMissionEventEventId: {
		AcceptMissionEvent event;
		if (!event.ParseFromString(payload)) {
			return false;
		}
		tlsEcs.dispatcher.enqueue(event);
		return true;
	}
	case AfterEnterSceneEventId: {
		AfterEnterScene event;
		if (!event.ParseFromString(payload)) {
			return false;
		}
		tlsEcs.dispatcher.enqueue(event);
		return true;
	}
	case AfterLeaveSceneEventId: {
		AfterLeaveScene event;
		if (!event.ParseFromString(payload)) {
			return false;
		}
		tlsEcs.dispatcher.enqueue(event);
		return true;
	}
	case BeKillEventEventId: {
		BeKillEvent event;
		if (!event.ParseFromString(payload)) {
			return false;
		}
		tlsEcs.dispatcher.enqueue(event);
		return true;
	}
	case BeforeEnterSceneEventId: {
		BeforeEnterScene event;
		if (!event.ParseFromString(payload)) {
			return false;
		}
		tlsEcs.dispatcher.enqueue(event);
		return true;
	}
	case BeforeLeaveSceneEventId: {
		BeforeLeaveScene event;
		if (!event.ParseFromString(payload)) {
			return false;
		}
		tlsEcs.dispatcher.enqueue(event);
		return true;
	}
	case BuffTestEventEventId: {
		BuffTestEvent event;
		if (!event.ParseFromString(payload)) {
			return false;
		}
		tlsEcs.dispatcher.enqueue(event);
		return true;
	}
	case CombatStateAddedPbEventEventId: {
		CombatStateAddedPbEvent event;
		if (!event.ParseFromString(payload)) {
			return false;
		}
		tlsEcs.dispatcher.enqueue(event);
		return true;
	}
	case CombatStateRemovedPbEventEventId: {
		CombatStateRemovedPbEvent event;
		if (!event.ParseFromString(payload)) {
			return false;
		}
		tlsEcs.dispatcher.enqueue(event);
		return true;
	}
	case ConnectToNodePbEventEventId: {
		ConnectToNodePbEvent event;
		if (!event.ParseFromString(payload)) {
			return false;
		}
		tlsEcs.dispatcher.enqueue(event);
		return true;
	}
	case ContractsKafkaBindSessionEventEventId: {
		contracts::kafka::BindSessionEvent event;
		if (!event.ParseFromString(payload)) {
			return false;
		}
		tlsEcs.dispatcher.enqueue(event);
		return true;
	}
	case ContractsKafkaKickPlayerEventEventId: {
		contracts::kafka::KickPlayerEvent event;
		if (!event.ParseFromString(payload)) {
			return false;
		}
		tlsEcs.dispatcher.enqueue(event);
		return true;
	}
	case ContractsKafkaPlayerDisconnectedEventEventId: {
		contracts::kafka::PlayerDisconnectedEvent event;
		if (!event.ParseFromString(payload)) {
			return false;
		}
		tlsEcs.dispatcher.enqueue(event);
		return true;
	}
	case ContractsKafkaPlayerLeaseExpiredEventEventId: {
		contracts::kafka::PlayerLeaseExpiredEvent event;
		if (!event.ParseFromString(payload)) {
			return false;
		}
		tlsEcs.dispatcher.enqueue(event);
		return true;
	}
	case ContractsKafkaPlayerLifecycleCommandEventId: {
		contracts::kafka::PlayerLifecycleCommand event;
		if (!event.ParseFromString(payload)) {
			return false;
		}
		tlsEcs.dispatcher.enqueue(event);
		return true;
	}
	case ContractsKafkaRoutePlayerEventEventId: {
		contracts::kafka::RoutePlayerEvent event;
		if (!event.ParseFromString(payload)) {
			return false;
		}
		tlsEcs.dispatcher.enqueue(event);
		return true;
	}
	case InitializeActorComponentsEventEventId: {
		InitializeActorComponentsEvent event;
		if (!event.ParseFromString(payload)) {
			return false;
		}
		tlsEcs.dispatcher.enqueue(event);
		return true;
	}
	case InitializeNpcComponentsEventEventId: {
		InitializeNpcComponentsEvent event;
		if (!event.ParseFromString(payload)) {
			return false;
		}
		tlsEcs.dispatcher.enqueue(event);
		return true;
	}
	case InitializePlayerComponentsEventEventId: {
		InitializePlayerComponentsEvent event;
		if (!event.ParseFromString(payload)) {
			return false;
		}
		tlsEcs.dispatcher.enqueue(event);
		return true;
	}
	case InterruptCurrentStatePbEventEventId: {
		InterruptCurrentStatePbEvent event;
		if (!event.ParseFromString(payload)) {
			return false;
		}
		tlsEcs.dispatcher.enqueue(event);
		return true;
	}
	case MissionConditionEventEventId: {
		MissionConditionEvent event;
		if (!event.ParseFromString(payload)) {
			return false;
		}
		tlsEcs.dispatcher.enqueue(event);
		return true;
	}
	case OnAcceptedMissionEventEventId: {
		OnAcceptedMissionEvent event;
		if (!event.ParseFromString(payload)) {
			return false;
		}
		tlsEcs.dispatcher.enqueue(event);
		return true;
	}
	case OnConnect2CentrePbEventEventId: {
		OnConnect2CentrePbEvent event;
		if (!event.ParseFromString(payload)) {
			return false;
		}
		tlsEcs.dispatcher.enqueue(event);
		return true;
	}
	case OnConnect2LoginEventId: {
		OnConnect2Login event;
		if (!event.ParseFromString(payload)) {
			return false;
		}
		tlsEcs.dispatcher.enqueue(event);
		return true;
	}
	case OnMissionAwardEventEventId: {
		OnMissionAwardEvent event;
		if (!event.ParseFromString(payload)) {
			return false;
		}
		tlsEcs.dispatcher.enqueue(event);
		return true;
	}
	case OnNodeAddPbEventEventId: {
		OnNodeAddPbEvent event;
		if (!event.ParseFromString(payload)) {
			return false;
		}
		tlsEcs.dispatcher.enqueue(event);
		return true;
	}
	case OnNodeConnectedPbEventEventId: {
		OnNodeConnectedPbEvent event;
		if (!event.ParseFromString(payload)) {
			return false;
		}
		tlsEcs.dispatcher.enqueue(event);
		return true;
	}
	case OnNodeRemovePbEventEventId: {
		OnNodeRemovePbEvent event;
		if (!event.ParseFromString(payload)) {
			return false;
		}
		tlsEcs.dispatcher.enqueue(event);
		return true;
	}
	case OnSceneCreatedEventId: {
		OnSceneCreated event;
		if (!event.ParseFromString(payload)) {
			return false;
		}
		tlsEcs.dispatcher.enqueue(event);
		return true;
	}
	case OnSceneDestroyedEventId: {
		OnSceneDestroyed event;
		if (!event.ParseFromString(payload)) {
			return false;
		}
		tlsEcs.dispatcher.enqueue(event);
		return true;
	}
	case OnServerStartEventId: {
		OnServerStart event;
		if (!event.ParseFromString(payload)) {
			return false;
		}
		tlsEcs.dispatcher.enqueue(event);
		return true;
	}
	case PlayerMigrationPbEventEventId: {
		PlayerMigrationPbEvent event;
		if (!event.ParseFromString(payload)) {
			return false;
		}
		tlsEcs.dispatcher.enqueue(event);
		return true;
	}
	case PlayerUpgradeEventEventId: {
		PlayerUpgradeEvent event;
		if (!event.ParseFromString(payload)) {
			return false;
		}
		tlsEcs.dispatcher.enqueue(event);
		return true;
	}
	case RegisterPlayerEventEventId: {
		RegisterPlayerEvent event;
		if (!event.ParseFromString(payload)) {
			return false;
		}
		tlsEcs.dispatcher.enqueue(event);
		return true;
	}
	case S2CEnterSceneEventId: {
		S2CEnterScene event;
		if (!event.ParseFromString(payload)) {
			return false;
		}
		tlsEcs.dispatcher.enqueue(event);
		return true;
	}
	case SkillExecutedEventEventId: {
		SkillExecutedEvent event;
		if (!event.ParseFromString(payload)) {
			return false;
		}
		tlsEcs.dispatcher.enqueue(event);
		return true;
	}
	default:
		return false;
	}
}
