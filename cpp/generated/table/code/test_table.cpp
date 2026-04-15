#include "google/protobuf/util/json_util.h"
#include "core/utils/file/file2string.h"
#include "table/proto/tip/common_error_tip.pb.h"
#include "table/code/test_table.h"

std::string GetConfigDir();
bool UseProtoBinaryTables();

void TestTableManager::Load() {
    auto snap = std::make_unique<Snapshot>();

    if (UseProtoBinaryTables()) {
        const std::string path = GetConfigDir() + "test.pb";
        const auto contents = File2String(path);
        if (!snap->data.ParseFromString(contents)) {
            LOG_FATAL << "Test binary parse failed: " << path;
        }
    } else {
        const std::string path = GetConfigDir() + "test.json";
        const auto contents = File2String(path);
        if (const auto result = google::protobuf::util::JsonStringToMessage(contents.data(), &snap->data); !result.ok()) {
            LOG_FATAL << "Test" << result.message().data();
        }
    }

    for (int32_t i = 0; i < snap->data.data_size(); ++i) {
        const auto& row_data = snap->data.data(i);
        snap->kvData.emplace(row_data.id(), &row_data);
        for (const auto& elem : row_data.effect()) {
            snap->idxeffect.emplace(elem, &row_data);
        }
    }

    snapshot_ = std::move(snap);
}

std::pair<const TestTable*, uint32_t> TestTableManager::FindById(const uint32_t tableId) {
    const auto& snap = GetSnapshot();
    const auto it = snap.kvData.find(tableId);
    if (it == snap.kvData.end()) {
        LOG_ERROR << "Test table not found for ID: " << tableId;
        return {nullptr, kInvalidTableId};
    }
    return {it->second, kSuccess};
}

std::pair<const TestTable*, uint32_t> TestTableManager::FindByIdSilent(const uint32_t tableId) {
    const auto& snap = GetSnapshot();
    const auto it = snap.kvData.find(tableId);
    if (it == snap.kvData.end()) {
        return {nullptr, kInvalidTableId};
    }
    return {it->second, kSuccess};
}
