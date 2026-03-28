
#include "google/protobuf/util/json_util.h"
#include "core/utils/file/file2string.h"
#include "table/proto/tip/common_error_tip.pb.h"
#include "table/code/mission_table.h"

std::string GetConfigDir();

void MissionTableManager::Load() {
    data_.Clear();

    std::string path = GetConfigDir() + "mission.json";
    const auto contents = File2String(path);
    if (const auto result = google::protobuf::util::JsonStringToMessage(contents.data(), &data_); !result.ok()) {
        LOG_FATAL << "Mission" << result.message().data();
    }

    for (int32_t i = 0; i < data_.data_size(); ++i) {
        const auto& row_data = data_.data(i);
        kv_data_.emplace(row_data.id(), &row_data);
        for (const auto& elem : row_data.condition_id()) {
            idx_condition_id_.emplace(elem, &row_data);
        }
        for (const auto& elem : row_data.next_mission_id()) {
            idx_next_mission_id_.emplace(elem, &row_data);
        }
    }

}

std::pair<const MissionTable*, uint32_t> MissionTableManager::GetTable(const uint32_t tableId) {
    const auto it = kv_data_.find(tableId);
    if (it == kv_data_.end()) {
        LOG_ERROR << "Mission table not found for ID: " << tableId;
        return {nullptr, kInvalidTableId};
    }
    return {it->second, kSuccess};
}

std::pair<const MissionTable*, uint32_t> MissionTableManager::GetTableWithoutErrorLogging(const uint32_t tableId) {
    const auto it = kv_data_.find(tableId);
    if (it == kv_data_.end()) {
        return {nullptr, kInvalidTableId};
    }
    return {it->second, kSuccess};
}

