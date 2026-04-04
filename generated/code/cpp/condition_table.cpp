
#include "google/protobuf/util/json_util.h"
#include "core/utils/file/file2string.h"
#include "table/proto/tip/common_error_tip.pb.h"
#include "table/code/condition_table.h"

std::string GetConfigDir();

void ConditionTableManager::Load() {
    data_.Clear();

    std::string path = GetConfigDir() + "condition.json";
    const auto contents = File2String(path);
    if (const auto result = google::protobuf::util::JsonStringToMessage(contents.data(), &data_); !result.ok()) {
        LOG_FATAL << "Condition" << result.message().data();
    }

    for (int32_t i = 0; i < data_.data_size(); ++i) {
        const auto& row_data = data_.data(i);
        kv_data_.emplace(row_data.id(), &row_data);

        if (row_data.comparison_op() > 4) {
            LOG_FATAL << "Condition table id=" << row_data.id()
                      << " has invalid comparison_op=" << row_data.comparison_op()
                      << " (valid range: 0-4)";
        }

        for (const auto& elem : row_data.condition1()) {
            idx_condition1_.emplace(elem, &row_data);
        }
        for (const auto& elem : row_data.condition2()) {
            idx_condition2_.emplace(elem, &row_data);
        }
        for (const auto& elem : row_data.condition3()) {
            idx_condition3_.emplace(elem, &row_data);
        }
        for (const auto& elem : row_data.condition4()) {
            idx_condition4_.emplace(elem, &row_data);
        }
    }

}

std::pair<const ConditionTable*, uint32_t> ConditionTableManager::GetTable(const uint32_t tableId) {
    const auto it = kv_data_.find(tableId);
    if (it == kv_data_.end()) {
        LOG_ERROR << "Condition table not found for ID: " << tableId;
        return {nullptr, kInvalidTableId};
    }
    return {it->second, kSuccess};
}

std::pair<const ConditionTable*, uint32_t> ConditionTableManager::GetTableWithoutErrorLogging(const uint32_t tableId) {
    const auto it = kv_data_.find(tableId);
    if (it == kv_data_.end()) {
        return {nullptr, kInvalidTableId};
    }
    return {it->second, kSuccess};
}

