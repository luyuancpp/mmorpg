
#include "player_database_loader.h"

#include "thread_context/registry_manager.h"
#include "proto/common/database/mysql_database_table.pb.h"
#include "modules/currency/comp/player_currency_comp.h"

void PlayerDatabaseMessageFieldsUnmarshal(entt::entity player, const player_database& message){
	tlsRegistryManager.actorRegistry.emplace<Transform>(player, message.transform());
	tlsRegistryManager.actorRegistry.emplace<PlayerUint64Comp>(player, message.uint64_pb_component());
	tlsRegistryManager.actorRegistry.emplace<PlayerSkillListComp>(player, message.skill_list());
	tlsRegistryManager.actorRegistry.emplace<PlayerUint32Comp>(player, message.uint32_pb_component());
	tlsRegistryManager.actorRegistry.emplace<BaseAttributesComp>(player, message.derived_attributes_component());
	tlsRegistryManager.actorRegistry.emplace<LevelComp>(player, message.level_component());
	tlsRegistryManager.actorRegistry.emplace<CurrencyComp>(player, message.currency());
	tlsRegistryManager.actorRegistry.emplace<PlayerCurrencyComp>(player);
}

void PlayerDatabaseMessageFieldsMarshal(entt::entity player, player_database& message){
	message.mutable_transform()->CopyFrom(tlsRegistryManager.actorRegistry.get_or_emplace<Transform>(player));
	message.mutable_uint64_pb_component()->CopyFrom(tlsRegistryManager.actorRegistry.get_or_emplace<PlayerUint64Comp>(player));
	message.mutable_skill_list()->CopyFrom(tlsRegistryManager.actorRegistry.get_or_emplace<PlayerSkillListComp>(player));
	message.mutable_uint32_pb_component()->CopyFrom(tlsRegistryManager.actorRegistry.get_or_emplace<PlayerUint32Comp>(player));
	message.mutable_derived_attributes_component()->CopyFrom(tlsRegistryManager.actorRegistry.get_or_emplace<BaseAttributesComp>(player));
	message.mutable_level_component()->CopyFrom(tlsRegistryManager.actorRegistry.get_or_emplace<LevelComp>(player));
	message.mutable_currency()->CopyFrom(tlsRegistryManager.actorRegistry.get_or_emplace<CurrencyComp>(player));
}


