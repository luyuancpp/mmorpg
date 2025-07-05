
#include "thread_local/storage.h"
#include "proto/db/mysql_database_table.pb.h"

void Player_databaseMessageFieldsUnmarshal(entt::entity player, const player_database& message){
	tls.actorRegistry.emplace<Transform>(player, message.transform());
	tls.actorRegistry.emplace<PlayerUint64PBComponent>(player, message.uint64_pb_component());
	tls.actorRegistry.emplace<PlayerSkillListPBComponent>(player, message.skill_list());
	tls.actorRegistry.emplace<PlayerUint32PbComponent>(player, message.uint32_pb_component());
	tls.actorRegistry.emplace<BaseAttributesPbComponent>(player, message.derived_attributes_component());
	tls.actorRegistry.emplace<LevelPbComponent>(player, message.level_component());
}

void Player_databaseMessageFieldsMarshal(entt::entity player, player_database& message){
	message.mutable_transform()->CopyFrom(tls.actorRegistry.get<Transform>(player));
	message.mutable_uint64_pb_component()->CopyFrom(tls.actorRegistry.get<PlayerUint64PBComponent>(player));
	message.mutable_skill_list()->CopyFrom(tls.actorRegistry.get<PlayerSkillListPBComponent>(player));
	message.mutable_uint32_pb_component()->CopyFrom(tls.actorRegistry.get<PlayerUint32PbComponent>(player));
	message.mutable_derived_attributes_component()->CopyFrom(tls.actorRegistry.get<BaseAttributesPbComponent>(player));
	message.mutable_level_component()->CopyFrom(tls.actorRegistry.get<LevelPbComponent>(player));
}
