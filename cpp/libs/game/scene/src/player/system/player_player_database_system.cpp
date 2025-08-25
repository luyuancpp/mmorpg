
#include "thread_local/registry_manager.h"
#include "proto/db/mysql_database_table.pb.h"

void PlayerDatabaseMessageFieldsUnmarshal(entt::entity player, const player_database& message){
	tlsRegistryManager.actorRegistry.emplace<Transform>(player, message.transform());
	tlsRegistryManager.actorRegistry.emplace<PlayerUint64PBComponent>(player, message.uint64_pb_component());
	tlsRegistryManager.actorRegistry.emplace<PlayerSkillListPBComponent>(player, message.skill_list());
	tlsRegistryManager.actorRegistry.emplace<PlayerUint32PbComponent>(player, message.uint32_pb_component());
	tlsRegistryManager.actorRegistry.emplace<BaseAttributesPbComponent>(player, message.derived_attributes_component());
	tlsRegistryManager.actorRegistry.emplace<LevelPbComponent>(player, message.level_component());
}

void PlayerDatabaseMessageFieldsMarshal(entt::entity player, player_database& message){
	message.mutable_transform()->CopyFrom(tlsRegistryManager.actorRegistry.get<Transform>(player));
	message.mutable_uint64_pb_component()->CopyFrom(tlsRegistryManager.actorRegistry.get<PlayerUint64PBComponent>(player));
	message.mutable_skill_list()->CopyFrom(tlsRegistryManager.actorRegistry.get<PlayerSkillListPBComponent>(player));
	message.mutable_uint32_pb_component()->CopyFrom(tlsRegistryManager.actorRegistry.get<PlayerUint32PbComponent>(player));
	message.mutable_derived_attributes_component()->CopyFrom(tlsRegistryManager.actorRegistry.get<BaseAttributesPbComponent>(player));
	message.mutable_level_component()->CopyFrom(tlsRegistryManager.actorRegistry.get<LevelPbComponent>(player));
}

