#include "google/protobuf/util/json_util.h"
#include "core/utils/file/file2string.h"
#include "table/proto/tip/common_error_tip.pb.h"
#include "table/code/world_table.h"

std::string GetConfigDir();
bool UseProtoBinaryTables();

void WorldTableManager::Load() {
    auto snap = std::make_unique<Snapshot>();

    if (UseProtoBinaryTables()) {
        const std::string path = GetConfigDir() + "world.pb";
        const auto contents = File2String(path);
        if (!snap->data.ParseFromString(contents)) {
            LOG_FATAL << "World binary parse failed: " << path;
        }
    } else {
        const std::string path = GetConfigDir() + "world.json";
        const auto contents = File2String(path);
        if (const auto result = google::protobuf::util::JsonStringToMessage(contents.data(), &snap->data); !result.ok()) {
            LOG_FATAL << "World" << result.message().data();
        }
    }

    for (int32_t i = 0; i < snap->data.data_size(); ++i) {
        const auto& row_data = snap->data.data(i);
        snap->kvData.emplace(row_data.id(), &row_data);
    }

    snapshot_ = std::move(snap);
}

std::pair<const WorldTable*, uint32_t> WorldTableManager::FindById(const uint32_t tableId) {
    const auto& snap = GetSnapshot();
    const auto it = snap.kvData.find(tableId);
    if (it == snap.kvData.end()) {
        LOG_ERROR << "World table not found for ID: " << tableId;
        return {nullptr, kInvalidTableId};
    }
    return {it->second, kSuccess};
}

std::pair<const WorldTable*, uint32_t> WorldTableManager::FindByIdSilent(const uint32_t tableId) {
    const auto& snap = GetSnapshot();
    const auto it = snap.kvData.find(tableId);
    if (it == snap.kvData.end()) {
        return {nullptr, kInvalidTableId};
    }
    return {it->second, kSuccess};
}
