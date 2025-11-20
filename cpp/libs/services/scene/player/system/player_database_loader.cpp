
#include "threading/registry_manager.h"
#include "proto/logic/database/mysql_database_table.pb.h"

void PlayerDatabaseMessageFieldsUnmarshal(entt::entity player, const player_database& message){
	tlsRegistryManager.actorRegistry.get_or_emplace<Transform>(player, message.transform());
	tlsRegistryManager.actorRegistry.get_or_emplace<PlayerUint64PBComponent>(player, message.uint64_pb_component());
	tlsRegistryManager.actorRegistry.get_or_emplace<PlayerSkillListPBComponent>(player, message.skill_list());
	tlsRegistryManager.actorRegistry.get_or_emplace<PlayerUint32PbComponent>(player, message.uint32_pb_component());
	tlsRegistryManager.actorRegistry.get_or_emplace<BaseAttributesPbComponent>(player, message.derived_attributes_component());
	tlsRegistryManager.actorRegistry.get_or_emplace<LevelPbComponent>(player, message.level_component());
}

void PlayerDatabaseMessageFieldsMarshal(entt::entity player, player_database& message){
	message.mutable_transform()->CopyFrom(tlsRegistryManager.actorRegistry.get_or_emplace<Transform>(player));
	message.mutable_uint64_pb_component()->CopyFrom(tlsRegistryManager.actorRegistry.get_or_emplace<PlayerUint64PBComponent>(player));
	message.mutable_skill_list()->CopyFrom(tlsRegistryManager.actorRegistry.get_or_emplace<PlayerSkillListPBComponent>(player));
	message.mutable_uint32_pb_component()->CopyFrom(tlsRegistryManager.actorRegistry.get_or_emplace<PlayerUint32PbComponent>(player));
	message.mutable_derived_attributes_component()->CopyFrom(tlsRegistryManager.actorRegistry.get_or_emplace<BaseAttributesPbComponent>(player));
	message.mutable_level_component()->CopyFrom(tlsRegistryManager.actorRegistry.get_or_emplace<LevelPbComponent>(player));
}

