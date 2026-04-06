
#include "google/protobuf/util/json_util.h"
#include "core/utils/file/file2string.h"
#include "table/proto/tip/common_error_tip.pb.h"
#include "table/code/testmultikey_table.h"

std::string GetConfigDir();
bool UseProtoBinaryTables();

void TestMultiKeyTableManager::Load() {
    data_.Clear();

    if (UseProtoBinaryTables()) {
        const std::string path = GetConfigDir() + "testmultikey.pb";
        const auto contents = File2String(path);
        if (!data_.ParseFromString(contents)) {
            LOG_FATAL << "TestMultiKey binary parse failed: " << path;
        }
    } else {
        const std::string path = GetConfigDir() + "testmultikey.json";
        const auto contents = File2String(path);
        if (const auto result = google::protobuf::util::JsonStringToMessage(contents.data(), &data_); !result.ok()) {
            LOG_FATAL << "TestMultiKey" << result.message().data();
        }
    }

    for (int32_t i = 0; i < data_.data_size(); ++i) {
        const auto& row_data = data_.data(i);
        kv_data_.emplace(row_data.id(), &row_data);
        kv_string_keydata_.emplace(row_data.string_key(), &row_data);
        kv_uint32_keydata_.emplace(row_data.uint32_key(), &row_data);
        kv_int32_keydata_.emplace(row_data.int32_key(), &row_data);
        kv_m_string_keydata_.emplace(row_data.m_string_key(), &row_data);
        kv_m_uint32_keydata_.emplace(row_data.m_uint32_key(), &row_data);
        kv_m_int32_keydata_.emplace(row_data.m_int32_key(), &row_data);
        for (const auto& elem : row_data.effect()) {
            idx_effect_.emplace(elem, &row_data);
        }
    }
}

std::pair<const TestMultiKeyTable*, uint32_t> TestMultiKeyTableManager::GetTable(const uint32_t tableId) {
    const auto it = kv_data_.find(tableId);
    if (it == kv_data_.end()) {
        LOG_ERROR << "TestMultiKey table not found for ID: " << tableId;
        return {nullptr, kInvalidTableId};
    }
    return {it->second, kSuccess};
}

std::pair<const TestMultiKeyTable*, uint32_t> TestMultiKeyTableManager::GetTableWithoutErrorLogging(const uint32_t tableId) {
    const auto it = kv_data_.find(tableId);
    if (it == kv_data_.end()) {
        return {nullptr, kInvalidTableId};
    }
    return {it->second, kSuccess};
}

std::pair<const TestMultiKeyTable*, uint32_t> TestMultiKeyTableManager::GetByString_key(const std::string& tableId) const {
    const auto it = kv_string_keydata_.find(tableId);
    if (it == kv_string_keydata_.end()) {
        LOG_ERROR << "TestMultiKey table not found for string_key: " << tableId;
        return {nullptr, kInvalidTableId};
    }
    return {it->second, kSuccess};
}

std::pair<const TestMultiKeyTable*, uint32_t> TestMultiKeyTableManager::GetByUint32_key(uint32_t tableId) const {
    const auto it = kv_uint32_keydata_.find(tableId);
    if (it == kv_uint32_keydata_.end()) {
        LOG_ERROR << "TestMultiKey table not found for uint32_key: " << tableId;
        return {nullptr, kInvalidTableId};
    }
    return {it->second, kSuccess};
}

std::pair<const TestMultiKeyTable*, uint32_t> TestMultiKeyTableManager::GetByInt32_key(int32_t tableId) const {
    const auto it = kv_int32_keydata_.find(tableId);
    if (it == kv_int32_keydata_.end()) {
        LOG_ERROR << "TestMultiKey table not found for int32_key: " << tableId;
        return {nullptr, kInvalidTableId};
    }
    return {it->second, kSuccess};
}

std::pair<const TestMultiKeyTable*, uint32_t> TestMultiKeyTableManager::GetByM_string_key(const std::string& tableId) const {
    const auto it = kv_m_string_keydata_.find(tableId);
    if (it == kv_m_string_keydata_.end()) {
        LOG_ERROR << "TestMultiKey table not found for m_string_key: " << tableId;
        return {nullptr, kInvalidTableId};
    }
    return {it->second, kSuccess};
}

std::pair<const TestMultiKeyTable*, uint32_t> TestMultiKeyTableManager::GetByM_uint32_key(uint32_t tableId) const {
    const auto it = kv_m_uint32_keydata_.find(tableId);
    if (it == kv_m_uint32_keydata_.end()) {
        LOG_ERROR << "TestMultiKey table not found for m_uint32_key: " << tableId;
        return {nullptr, kInvalidTableId};
    }
    return {it->second, kSuccess};
}

std::pair<const TestMultiKeyTable*, uint32_t> TestMultiKeyTableManager::GetByM_int32_key(int32_t tableId) const {
    const auto it = kv_m_int32_keydata_.find(tableId);
    if (it == kv_m_int32_keydata_.end()) {
        LOG_ERROR << "TestMultiKey table not found for m_int32_key: " << tableId;
        return {nullptr, kInvalidTableId};
    }
    return {it->second, kSuccess};
}
