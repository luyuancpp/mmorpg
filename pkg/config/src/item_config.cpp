#include "google/protobuf/util/json_util.h"
#include "src/util/file2string.h"
#include "common_error_tip.pb.h"
#include "item_config.h"

void ItemConfigurationTable::Load() {
    data_.Clear();
    const auto contents = File2String("config/generated/json/item.json");
    if (const auto result = google::protobuf::util::JsonStringToMessage(contents.data(), &data_); !result.ok()) {
        LOG_FATAL << "Item" << result.message().data();
    }

    for (int32_t i = 0; i < data_.data_size(); ++i) {
        const auto& row_data = data_.data(i);
        kv_data_.emplace(row_data.id(), &row_data);
    }
}

std::pair<const ItemTable*, uint32_t> ItemConfigurationTable::GetTable(const uint32_t tableId) {
    const auto it = kv_data_.find(tableId);
    if (it == kv_data_.end()) {
        LOG_ERROR << "Item table not found for ID: " << tableId;
        return { nullptr, kInvalidTableId };
    }
    return { it->second, kSuccess };
}

std::pair<const ItemTable*, uint32_t> ItemConfigurationTable::GetTableWithoutErrorLogging(const uint32_t tableId) {
    const auto it = kv_data_.find(tableId);
    if (it == kv_data_.end()) {
        return { nullptr, kInvalidTableId };
    }
    return { it->second, kSuccess };
}