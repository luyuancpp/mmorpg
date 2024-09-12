package handler

#include "thread_local/storage.h"
#include "proto/common/mysql_database_table.pb.h"

void Playerplayer_database_1Unmarshal(entt::entity player, const player_database& message){
	tls.registry.emplace<TYPE_UINT64>(player, message.player_id);
}

void Playerplayer_database_1Marshal(entt::entity player, const player_database& message){
	tls.registry.emplace<TYPE_UINT64>(player, message.player_id);
}
