#include "google/protobuf/util/json_util.h"
#include "core/utils/file/file2string.h"
#include "table/proto/tip/common_error_tip.pb.h"
#include "table/code/skillpermission_table.h"

std::string GetConfigDir();
bool UseProtoBinaryTables();

void SkillPermissionTableManager::Load() {
    data_.Clear();

    if (UseProtoBinaryTables()) {
        const std::string path = GetConfigDir() + "skillpermission.pb";
        const auto contents = File2String(path);
        if (!data_.ParseFromString(contents)) {
            LOG_FATAL << "SkillPermission binary parse failed: " << path;
        }
    } else {
        const std::string path = GetConfigDir() + "skillpermission.json";
        const auto contents = File2String(path);
        if (const auto result = google::protobuf::util::JsonStringToMessage(contents.data(), &data_); !result.ok()) {
            LOG_FATAL << "SkillPermission" << result.message().data();
        }
    }

    for (int32_t i = 0; i < data_.data_size(); ++i) {
        const auto& row_data = data_.data(i);
        kv_data_.emplace(row_data.id(), &row_data);
        for (const auto& elem : row_data.skill_type()) {
            idx_skill_type_.emplace(elem, &row_data);
        }
    }
}

std::pair<const SkillPermissionTable*, uint32_t> SkillPermissionTableManager::FindById(const uint32_t tableId) {
    const auto it = kv_data_.find(tableId);
    if (it == kv_data_.end()) {
        LOG_ERROR << "SkillPermission table not found for ID: " << tableId;
        return {nullptr, kInvalidTableId};
    }
    return {it->second, kSuccess};
}

std::pair<const SkillPermissionTable*, uint32_t> SkillPermissionTableManager::FindByIdSilent(const uint32_t tableId) {
    const auto it = kv_data_.find(tableId);
    if (it == kv_data_.end()) {
        return {nullptr, kInvalidTableId};
    }
    return {it->second, kSuccess};
}
