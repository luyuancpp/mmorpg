#include "google/protobuf/util/json_util.h"
#include "core/utils/file/file2string.h"
#include "table/proto/tip/common_error_tip.pb.h"
#include "table/code/test_table.h"

std::string GetConfigDir();
bool UseProtoBinaryTables();

void TestTableManager::Load() {
    data_.Clear();

    if (UseProtoBinaryTables()) {
        const std::string path = GetConfigDir() + "test.pb";
        const auto contents = File2String(path);
        if (!data_.ParseFromString(contents)) {
            LOG_FATAL << "Test binary parse failed: " << path;
        }
    } else {
        const std::string path = GetConfigDir() + "test.json";
        const auto contents = File2String(path);
        if (const auto result = google::protobuf::util::JsonStringToMessage(contents.data(), &data_); !result.ok()) {
            LOG_FATAL << "Test" << result.message().data();
        }
    }

    for (int32_t i = 0; i < data_.data_size(); ++i) {
        const auto& row_data = data_.data(i);
        kv_data_.emplace(row_data.id(), &row_data);
        for (const auto& elem : row_data.effect()) {
            idx_effect_.emplace(elem, &row_data);
        }
    }
}

std::pair<const TestTable*, uint32_t> TestTableManager::FindById(const uint32_t tableId) {
    const auto it = kv_data_.find(tableId);
    if (it == kv_data_.end()) {
        LOG_ERROR << "Test table not found for ID: " << tableId;
        return {nullptr, kInvalidTableId};
    }
    return {it->second, kSuccess};
}

std::pair<const TestTable*, uint32_t> TestTableManager::FindByIdSilent(const uint32_t tableId) {
    const auto it = kv_data_.find(tableId);
    if (it == kv_data_.end()) {
        return {nullptr, kInvalidTableId};
    }
    return {it->second, kSuccess};
}
