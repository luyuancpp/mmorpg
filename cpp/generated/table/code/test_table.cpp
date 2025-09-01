#include "google/protobuf/util/json_util.h"
#include "core/utils/file/file2string.h"
#include "table/proto/tip/common_error_tip.pb.h"
#include "table/code/test_table.h"

std::string GetConfigDir();

void TestTableManager::Load() {
    data_.Clear();

    std::string path = GetConfigDir() + "test.json";
    const auto contents = File2String(path);
    if (const auto result = google::protobuf::util::JsonStringToMessage(contents.data(), &data_); !result.ok()) {
        LOG_FATAL << "Test" << result.message().data();
    }

    for (int32_t i = 0; i < data_.data_size(); ++i) {
        const auto& row_data = data_.data(i);
        kv_data_.emplace(row_data.id(), &row_data);
    }
}

std::pair<const TestTable*, uint32_t> TestTableManager::GetTable(const uint32_t tableId) {
    const auto it = kv_data_.find(tableId);
    if (it == kv_data_.end()) {
        LOG_ERROR << "Test table not found for ID: " << tableId;
        return {nullptr, kInvalidTableId };
    }
    return {it->second, kSuccess };
}

std::pair<const TestTable*, uint32_t> TestTableManager::GetTableWithoutErrorLogging(const uint32_t tableId) {
    const auto it = kv_data_.find(tableId);
    if (it == kv_data_.end()) {
        return {nullptr, kInvalidTableId };
    }
    return {it->second, kSuccess };
}