
#include "thread_local/storage.h"
#include "proto/common/mysql_database_table.pb.h"

void Player_databaseUnmarshal(entt::entity player, const player_database& message){
	tls.registry.emplace<Transform>(player, message.transform());
	tls.registry.emplace<PlayerSkillListPBComp>(player, message.skill_list());
}

void Player_databaseMarshal(entt::entity player, const player_database& message){
	tls.registry.emplace<Transform>(player, message.transform());
	tls.registry.emplace<PlayerSkillListPBComp>(player, message.skill_list());
}
