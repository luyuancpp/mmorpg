package handler

#include "thread_local/storage.h"
#include "proto/common/mysql_database_table.pb.h"

void Playerplayer_databaseUnmarshal(entt::entity player, const player_database& message){
	tls.registry.emplace<TYPE_UINT64>(player, message.player_id);
	tls.registry.emplace<TYPE_MESSAGE>(player, message.transform);
	tls.registry.emplace<TYPE_INT64>(player, message.register_time);
	tls.registry.emplace<TYPE_MESSAGE>(player, message.skill_list);
}

void Playerplayer_databaseMarshal(entt::entity player, const player_database& message){
	tls.registry.emplace<TYPE_UINT64>(player, message.player_id);
	tls.registry.emplace<TYPE_MESSAGE>(player, message.transform);
	tls.registry.emplace<TYPE_INT64>(player, message.register_time);
	tls.registry.emplace<TYPE_MESSAGE>(player, message.skill_list);
}
