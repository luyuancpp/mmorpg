
#include "thread_local/storage.h"
#include "proto/common/mysql_database_table.pb.h"

void Player_databaseMessageFieldsUnmarshal(entt::entity player, const player_database& message){
	tls.registry.emplace<Transform>(player, message.transform());
	tls.registry.emplace<PlayerUint64PBComp>(player, message.uint64_pb_comp());
	tls.registry.emplace<PlayerSkillListPBComp>(player, message.skill_list());
	tls.registry.emplace<PlayerUint32PBComp>(player, message.uint32_pb_comp());
}

void Player_databaseMessageFieldsMarshal(entt::entity player, player_database& message){
	message.mutable_transform()->CopyFrom(tls.registry.get<Transform>(player));
	message.mutable_uint64_pb_comp()->CopyFrom(tls.registry.get<PlayerUint64PBComp>(player));
	message.mutable_skill_list()->CopyFrom(tls.registry.get<PlayerSkillListPBComp>(player));
	message.mutable_uint32_pb_comp()->CopyFrom(tls.registry.get<PlayerUint32PBComp>(player));
}
