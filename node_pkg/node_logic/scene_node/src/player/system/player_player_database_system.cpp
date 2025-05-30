
#include "thread_local/storage.h"
#include "proto/db/mysql_database_table.pb.h"

void Player_databaseMessageFieldsUnmarshal(entt::entity player, const player_database& message){
	tls.registry.emplace<Transform>(player, message.transform());
	tls.registry.emplace<PlayerUint64PBComponent>(player, message.uint64_pb_component());
	tls.registry.emplace<PlayerSkillListPBComponent>(player, message.skill_list());
	tls.registry.emplace<PlayerUint32PbComponent>(player, message.uint32_pb_component());
	tls.registry.emplace<BaseAttributesPbComponent>(player, message.derived_attributes_component());
	tls.registry.emplace<LevelPbComponent>(player, message.level_component());
}

void Player_databaseMessageFieldsMarshal(entt::entity player, player_database& message){
	message.mutable_transform()->CopyFrom(tls.registry.get<Transform>(player));
	message.mutable_uint64_pb_component()->CopyFrom(tls.registry.get<PlayerUint64PBComponent>(player));
	message.mutable_skill_list()->CopyFrom(tls.registry.get<PlayerSkillListPBComponent>(player));
	message.mutable_uint32_pb_component()->CopyFrom(tls.registry.get<PlayerUint32PbComponent>(player));
	message.mutable_derived_attributes_component()->CopyFrom(tls.registry.get<BaseAttributesPbComponent>(player));
	message.mutable_level_component()->CopyFrom(tls.registry.get<LevelPbComponent>(player));
}
