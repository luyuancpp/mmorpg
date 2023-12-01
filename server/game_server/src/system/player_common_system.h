#pragma once

#include "src/util/game_registry.h"
#include "src/game_logic/thread_local/thread_local_storage.h"
#include "src/redis_client/redis_client.h"

#include "common_proto/mysql_database_table.pb.h"

using PlayerDataRedisSystemPtr = std::unique_ptr<MessageAsyncClient<Guid, player_database>>;

class EnterGsInfo;

class PlayerCommonSystem
{
public:
	//如果异步加载过程中玩家断开链接了？会不会造成数据覆盖
	static void OnAsyncLoadPlayerDb(Guid player_id, player_database& message);

	static void OnAsyncSavePlayerDb(Guid player_id, player_database& message);

	static void SavePlayer(entt::entity player);

	static void EnterGs(const entt::entity player, const EnterGsInfo& enter_info);

	static void LeaveGs(entt::entity player);

	static void OnPlayerLogin(entt::entity player, uint32_t enter_gs_type);
	static void OnGateUpdateGameNodeSucceed(entt::entity player);


	static void RemovePlayerSession(Guid player_id);
	static void RemovePlayerSession(entt::entity player);
};
