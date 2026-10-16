
#include "thread_context/ecs_context.h"
#include "proto/common/database/mysql_database_table.pb.h"

void PlayerDatabase1MessageFieldsUnmarshal(entt::entity player, const player_database_1& message){
}

bool PlayerDatabase1MessageFieldsMarshal(entt::entity player, player_database_1& message){
	// 这张子表目前没有任何持久化组件(player_id 由调用方填),没有可缺的东西。
	return true;
}
