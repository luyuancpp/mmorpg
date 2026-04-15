#include "google/protobuf/util/json_util.h"
#include "core/utils/file/file2string.h"
#include "table/proto/tip/common_error_tip.pb.h"
#include "table/code/testmultikey_table.h"

std::string GetConfigDir();
bool UseProtoBinaryTables();

void TestMultiKeyTableManager::Load() {
    auto snap = std::make_unique<Snapshot>();

    if (UseProtoBinaryTables()) {
        const std::string path = GetConfigDir() + "testmultikey.pb";
        const auto contents = File2String(path);
        if (!snap->data.ParseFromString(contents)) {
            LOG_FATAL << "TestMultiKey binary parse failed: " << path;
        }
    } else {
        const std::string path = GetConfigDir() + "testmultikey.json";
        const auto contents = File2String(path);
        if (const auto result = google::protobuf::util::JsonStringToMessage(contents.data(), &snap->data); !result.ok()) {
            LOG_FATAL << "TestMultiKey" << result.message().data();
        }
    }

    for (int32_t i = 0; i < snap->data.data_size(); ++i) {
        const auto& row_data = snap->data.data(i);
        snap->kvData.emplace(row_data.id(), &row_data);
        snap->kvstring_key.emplace(row_data.string_key(), &row_data);
        snap->kvuint32_key.emplace(row_data.uint32_key(), &row_data);
        snap->kvint32_key.emplace(row_data.int32_key(), &row_data);
        snap->kvm_string_key.emplace(row_data.m_string_key(), &row_data);
        snap->kvm_uint32_key.emplace(row_data.m_uint32_key(), &row_data);
        snap->kvm_int32_key.emplace(row_data.m_int32_key(), &row_data);
        for (const auto& elem : row_data.effect()) {
            snap->idxeffect.emplace(elem, &row_data);
        }
    }

    snapshot_ = std::move(snap);
}

std::pair<const TestMultiKeyTable*, uint32_t> TestMultiKeyTableManager::FindById(const uint32_t tableId) {
    const auto& snap = GetSnapshot();
    const auto it = snap.kvData.find(tableId);
    if (it == snap.kvData.end()) {
        LOG_ERROR << "TestMultiKey table not found for ID: " << tableId;
        return {nullptr, kInvalidTableId};
    }
    return {it->second, kSuccess};
}

std::pair<const TestMultiKeyTable*, uint32_t> TestMultiKeyTableManager::FindByIdSilent(const uint32_t tableId) {
    const auto& snap = GetSnapshot();
    const auto it = snap.kvData.find(tableId);
    if (it == snap.kvData.end()) {
        return {nullptr, kInvalidTableId};
    }
    return {it->second, kSuccess};
}

std::pair<const TestMultiKeyTable*, uint32_t> TestMultiKeyTableManager::FindByString_key(const std::string& key) const {
    const auto& snap = GetSnapshot();
    const auto it = snap.kvstring_key.find(key);
    if (it == snap.kvstring_key.end()) {
        LOG_ERROR << "TestMultiKey table not found for string_key: " << key;
        return {nullptr, kInvalidTableId};
    }
    return {it->second, kSuccess};
}

std::pair<const TestMultiKeyTable*, uint32_t> TestMultiKeyTableManager::FindByUint32_key(uint32_t key) const {
    const auto& snap = GetSnapshot();
    const auto it = snap.kvuint32_key.find(key);
    if (it == snap.kvuint32_key.end()) {
        LOG_ERROR << "TestMultiKey table not found for uint32_key: " << key;
        return {nullptr, kInvalidTableId};
    }
    return {it->second, kSuccess};
}

std::pair<const TestMultiKeyTable*, uint32_t> TestMultiKeyTableManager::FindByInt32_key(int32_t key) const {
    const auto& snap = GetSnapshot();
    const auto it = snap.kvint32_key.find(key);
    if (it == snap.kvint32_key.end()) {
        LOG_ERROR << "TestMultiKey table not found for int32_key: " << key;
        return {nullptr, kInvalidTableId};
    }
    return {it->second, kSuccess};
}

std::pair<const TestMultiKeyTable*, uint32_t> TestMultiKeyTableManager::FindByM_string_key(const std::string& key) const {
    const auto& snap = GetSnapshot();
    const auto it = snap.kvm_string_key.find(key);
    if (it == snap.kvm_string_key.end()) {
        LOG_ERROR << "TestMultiKey table not found for m_string_key: " << key;
        return {nullptr, kInvalidTableId};
    }
    return {it->second, kSuccess};
}

std::pair<const TestMultiKeyTable*, uint32_t> TestMultiKeyTableManager::FindByM_uint32_key(uint32_t key) const {
    const auto& snap = GetSnapshot();
    const auto it = snap.kvm_uint32_key.find(key);
    if (it == snap.kvm_uint32_key.end()) {
        LOG_ERROR << "TestMultiKey table not found for m_uint32_key: " << key;
        return {nullptr, kInvalidTableId};
    }
    return {it->second, kSuccess};
}

std::pair<const TestMultiKeyTable*, uint32_t> TestMultiKeyTableManager::FindByM_int32_key(int32_t key) const {
    const auto& snap = GetSnapshot();
    const auto it = snap.kvm_int32_key.find(key);
    if (it == snap.kvm_int32_key.end()) {
        LOG_ERROR << "TestMultiKey table not found for m_int32_key: " << key;
        return {nullptr, kInvalidTableId};
    }
    return {it->second, kSuccess};
}
