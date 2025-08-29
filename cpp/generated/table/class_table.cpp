#include "google/protobuf/util/json_util.h"
#include "util/file2string.h"
#include "proto/table/tip/common_error_tip.pb.h"
#include "class_table.h"

std::string GetConfigDir();

void ClassTableManager::Load() {
    data_.Clear();

    std::string path = GetConfigDir() + "class.json";
    const auto contents = File2String(path);
    if (const auto result = google::protobuf::util::JsonStringToMessage(contents.data(), &data_); !result.ok()) {
        LOG_FATAL << "Class" << result.message().data();
    }

    for (int32_t i = 0; i < data_.data_size(); ++i) {
        const auto& row_data = data_.data(i);
        kv_data_.emplace(row_data.id(), &row_data);
    }
}

std::pair<const ClassTableTempPtr, uint32_t> ClassTableManager::GetTable(const uint32_t tableId) {
    const auto it = kv_data_.find(tableId);
    if (it == kv_data_.end()) {
        LOG_ERROR << "Class table not found for ID: " << tableId;
        return { ClassTableTempPtr(nullptr), kInvalidTableId };
    }
    return { ClassTableTempPtr(it->second), kSuccess };
}

std::pair<const ClassTableTempPtr, uint32_t> ClassTableManager::GetTableWithoutErrorLogging(const uint32_t tableId) {
    const auto it = kv_data_.find(tableId);
    if (it == kv_data_.end()) {
        return { ClassTableTempPtr(nullptr), kInvalidTableId };
    }
    return { ClassTableTempPtr(it->second), kSuccess };
}