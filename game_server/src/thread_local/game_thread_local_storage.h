#pragma once

#include "src/game_logic/player/player_list.h"
#include "src/network/node_info.h"
#include "src/network/gate_node.h"
#include "src/network/controller_node.h"
#include "src/system/redis_system.h"

#include "src/pb/pbc/common_proto/mysql_database_table.pb.h"

using PlayerDataRedisSystemPtr = std::unique_ptr<MessageAsyncClient<Guid, player_database>>;
using PlayerEntityPtrList = std::unordered_map<uint64_t, EntityPtr>;

class GameThreadLocalStorage
{
public:
	RedisSystem& redis_system() { return redis_system_; }
	ControllerNodes& controller_node() { return controller_node_; }
	GateNodes& gate_node() { return gate_nodes_; }
	GateSessionList& gate_sessions() { return gate_sessions_; }
	PlayerList& player_list() { return player_list_; }
	PlayerDataRedisSystemPtr& player_data_redis_system() { return player_data_redis_system_; }
	PlayerEntityPtrList& async_player_data() { return async_player_data_; }
private:
	RedisSystem redis_system_;
	ControllerNodes controller_node_;//controller ����ɾ������Ϊ�����gsһ�����ݸ������Ӽ��٣�ֻ�ߵײ���Զ�����
	GateNodes gate_nodes_;
	GateSessionList gate_sessions_;
	PlayerList player_list_;
	PlayerDataRedisSystemPtr player_data_redis_system_;
	PlayerEntityPtrList async_player_data_;
};

extern thread_local GameThreadLocalStorage game_tls;

