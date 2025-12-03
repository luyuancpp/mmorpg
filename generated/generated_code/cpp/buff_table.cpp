#include "google/protobuf/util/json_util.h"
#include "core/utils/file/file2string.h"
#include "table/proto/tip/common_error_tip.pb.h"
#include "table/code/buff_table.h"

std::string GetConfigDir();

void BuffTableManager::Load() {
    data_.Clear();

    std::string path = GetConfigDir() + "buff.json";
    const auto contents = File2String(path);
    if (const auto result = google::protobuf::util::JsonStringToMessage(contents.data(), &data_); !result.ok()) {
        LOG_FATAL << "Buff" << result.message().data();
    }

    for (int32_t i = 0; i < data_.data_size(); ++i) {
        const auto& row_data = data_.data(i);
        kv_data_.emplace(row_data.id(), &row_data);
    }
    expression_healthregeneration_.Init({
            "level", 
            "health"
    });
}

std::pair<const BuffTable*, uint32_t> BuffTableManager::GetTable(const uint32_t tableId) {
    const auto it = kv_data_.find(tableId);
    if (it == kv_data_.end()) {
        LOG_ERROR << "Buff table not found for ID: " << tableId;
        return {nullptr, kInvalidTableId };
    }
    return {it->second, kSuccess };
}

std::pair<const BuffTable*, uint32_t> BuffTableManager::GetTableWithoutErrorLogging(const uint32_t tableId) {
    const auto it = kv_data_.find(tableId);
    if (it == kv_data_.end()) {
        return {nullptr, kInvalidTableId };
    }
    return {it->second, kSuccess };
}