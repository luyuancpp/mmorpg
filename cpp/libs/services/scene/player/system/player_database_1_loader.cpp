
#include "thread_context/ecs_context.h"
#include "proto/common/database/mysql_database_table.pb.h"
#include "muduo/base/Logging.h"
#include "engine/core/utils/proto/unknown_field_carry.h"
#include "services/scene/player/comp/player_persist_carry_comp.h"

void PlayerDatabase1MessageFieldsUnmarshal(entt::entity player, const player_database_1& message){
	// 这张子表目前没有业务组件,只有顶层未知字段(新版本加的整列)与加载世代要原样带回
	// (docs/design/mixed-version-persist-field-protection.md §4.2 L1)。持久数据类,唯一创建点。
	tlsEcs.actorRegistry.emplace<PlayerDatabase1CarryComp>(
		player, PlayerDatabase1CarryComp{unknown_field_carry::Capture(message), message.persist_schema_gen()});
}

bool PlayerDatabase1MessageFieldsMarshal(entt::entity player, player_database_1& message){
	// player_id 由调用方填。载体缺失时整份拒存:补空等于把新版本写进来的整列抹掉。
	const auto* carry = tlsEcs.actorRegistry.try_get<PlayerDatabase1CarryComp>(player);
	if (carry == nullptr)
	{
		LOG_ERROR << "[PlayerSave] Persisted component(s) missing, refusing to marshal player_database_1"
				  << " (load invariant broken; nothing is written): entity=" << entt::to_integral(player)
				  << " missing=PlayerDatabase1CarryComp";
		return false;
	}
	if (!unknown_field_carry::Restore(carry->topLevelUnknown, message))
	{
		LOG_ERROR << "[PlayerSave] carried top-level unknown fields of player_database_1 are corrupt; refusing to marshal: entity="
				  << entt::to_integral(player);
		return false;
	}
	return true;
}
