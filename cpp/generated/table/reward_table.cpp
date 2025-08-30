#include "google/protobuf/util/json_util.h"
#include "util/file2string.h"
#include "proto/table/tip/common_error_tip.pb.h"
#include "reward_table.h"

std::string GetConfigDir();

void RewardTableManager::Load() {
    data_.Clear();

    std::string path = GetConfigDir() + "reward.json";
    const auto contents = File2String(path);
    if (const auto result = google::protobuf::util::JsonStringToMessage(contents.data(), &data_); !result.ok()) {
        LOG_FATAL << "Reward" << result.message().data();
    }

    for (int32_t i = 0; i < data_.data_size(); ++i) {
        const auto& row_data = data_.data(i);
        kv_data_.emplace(row_data.id(), &row_data);
    }
}

std::pair< RewardTableTempPtr, uint32_t> RewardTableManager::GetTable(const uint32_t tableId) {
    const auto it = kv_data_.find(tableId);
    if (it == kv_data_.end()) {
        LOG_ERROR << "Reward table not found for ID: " << tableId;
        return { RewardTableTempPtr(nullptr), kInvalidTableId };
    }
    return { RewardTableTempPtr(it->second), kSuccess };
}

std::pair< RewardTableTempPtr, uint32_t> RewardTableManager::GetTableWithoutErrorLogging(const uint32_t tableId) {
    const auto it = kv_data_.find(tableId);
    if (it == kv_data_.end()) {
        return { RewardTableTempPtr(nullptr), kInvalidTableId };
    }
    return { RewardTableTempPtr(it->second), kSuccess };
}