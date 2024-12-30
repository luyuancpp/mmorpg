#pragma once
#include <cstdint>
#include <memory>
#include <unordered_map>
#include "config_expression/config_expression.h"
#include "muduo/base/Logging.h"
#include "reward_config.pb.h"


class RewardConfigurationTable {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const RewardTable*>;
    static RewardConfigurationTable& Instance() { static RewardConfigurationTable instance; return instance; }
    const RewardTabledData& All() const { return data_; }
    std::pair<const RewardTable*, uint32_t> GetTable(uint32_t tableId);
    const KeyValueDataType& KeyValueData() const { return kv_data_; }
    void Load();


private:
    RewardTabledData data_;
    KeyValueDataType kv_data_;

};

inline const RewardTabledData& GetRewardAllTable() { return RewardConfigurationTable::Instance().All(); }

#define FetchAndValidateRewardTable(tableId) \
const auto [rewardTable, fetchResult] = RewardConfigurationTable::Instance().GetTable(tableId); \
do {if (!(rewardTable)) { LOG_ERROR << "Reward table not found for ID: " << tableId;return (fetchResult); }} while (0)

#define FetchAndValidateCustomRewardTable(prefix, tableId) \
const auto [##prefix##RewardTable, prefix##fetchResult] = RewardConfigurationTable::Instance().GetTable(tableId); \
do {if (!(##prefix##RewardTable)) { LOG_ERROR << "Reward table not found for ID: " << tableId;return (prefix##fetchResult); }} while (0)

#define FetchRewardTableOrReturnCustom(tableId, customReturnValue) \
const auto [rewardTable, fetchResult] = RewardConfigurationTable::Instance().GetTable(tableId); \
do {if (!(rewardTable)) { LOG_ERROR << "Reward table not found for ID: " << tableId;return (customReturnValue); }} while (0)

#define FetchRewardTableOrReturnVoid(tableId) \
const auto [rewardTable, fetchResult] = RewardConfigurationTable::Instance().GetTable(tableId); \
do {if (!(rewardTable)) { LOG_ERROR << "Reward table not found for ID: " << tableId;return ;}} while (0)

#define FetchRewardTableOrContinue(tableId) \
const auto [rewardTable, fetchResult] = RewardConfigurationTable::Instance().GetTable(tableId); \
do { if (!(rewardTable)) { LOG_ERROR << "Reward table not found for ID: " << tableId;continue; }} while (0)

#define FetchRewardTableOrReturnFalse(tableId) \
const auto [rewardTable, fetchResult] = RewardConfigurationTable::Instance().GetTable(tableId); \
do {if (!(rewardTable)) { LOG_ERROR << "Reward table not found for ID: " << tableId;return false; }} while (0)