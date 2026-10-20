// 由 proto 生成器(tools/proto_generator/protogen/internal/persistschema)按 proto/persist_schema_gen.txt 生成,不要手改。
// 设计:docs/design/mixed-version-persist-field-protection.md §3、§4.3。
#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace persist_schema
{
// 本二进制认识的玩家持久 schema 世代。存盘给两张玩家表盖章 max(kPersistSchemaGen, 加载世代)(§3.3)。
inline constexpr uint32_t kPersistSchemaGen = 1;

// 显式冻结的持久消息类型全名(§3.2 R1)。往返一致性测试不在这些类型的节点上注入未知字段,但继续向下遍历。
inline constexpr std::array<std::string_view, 6> kFrozenMessageTypes = {
    "BagAllData",
    "BagAllData.DynamicBagData",
    "ItemEntry",
    "PlayerAllData",
    "QuestAllData",
    "QuestScopeData",
};

// 存盘时不原样携带的持久字段 "<消息全名>.<字段名>"(§3.2)。往返一致性测试不经由它们注入。
inline constexpr std::array<std::string_view, 8> kUncarriedFields = {
    "ItemEntry.equip",
    "PlayerAllData.bag_data",
    "PlayerAllData.mail_data",
    "PlayerAllData.quest_data",
    "QuestAllData.active",
    "player_database.merge_state",
    "player_database.stress_test_probe",
    "player_database_1.stress_test_probe",
};
} // namespace persist_schema
