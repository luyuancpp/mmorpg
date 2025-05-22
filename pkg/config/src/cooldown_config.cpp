#include "google/protobuf/util/json_util.h"
#include "src/util/file2string.h"
#include "common_error_tip.pb.h"
#include "cooldown_config.h"

void CooldownConfigurationTable::Load() {
    data_.Clear();
    const auto contents = File2String("config/generated/json/cooldown.json");
    if (const auto result = google::protobuf::util::JsonStringToMessage(contents.data(), &data_); !result.ok()) {
        LOG_FATAL << "Cooldown" << result.message().data();
    }

    for (int32_t i = 0; i < data_.data_size(); ++i) {
        const auto& row_data = data_.data(i);
        kv_data_.emplace(row_data.id(), &row_data);
    }
}

std::pair<const CooldownTable*, uint32_t> CooldownConfigurationTable::GetTable(const uint32_t tableId) {
    const auto it = kv_data_.find(tableId);
    if (it == kv_data_.end()) {
        LOG_ERROR << "Cooldown table not found for ID: " << tableId;
        return { nullptr, kInvalidTableId };
    }
    return { it->second, kSuccess };
}

std::pair<const CooldownTable*, uint32_t> CooldownConfigurationTable::GetTableWithoutErrorLogging(const uint32_t tableId) {
    const auto it = kv_data_.find(tableId);
    if (it == kv_data_.end()) {
        return { nullptr, kInvalidTableId };
    }
    return { it->second, kSuccess };
}