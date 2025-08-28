#include "google/protobuf/util/json_util.h"
#include "util/file2string.h"
#include "proto/table/common_error_tip.pb.h"
#include "testmultikey_table.h"

std::string GetConfigDir();

void TestMultiKeyConfigurationTable::Load() {
    data_.Clear();

    std::string path = GetConfigDir() + "testmultikey.json";
    const auto contents = File2String(path);
    if (const auto result = google::protobuf::util::JsonStringToMessage(contents.data(), &data_); !result.ok()) {
        LOG_FATAL << "TestMultiKey" << result.message().data();
    }

    for (int32_t i = 0; i < data_.data_size(); ++i) {
        const auto& row_data = data_.data(i);
        kv_data_.emplace(row_data.id(), &row_data);
        kv_stringkeydata_.emplace(row_data.stringkey(), &row_data);
        kv_uint32keydata_.emplace(row_data.uint32key(), &row_data);
        kv_in32keydata_.emplace(row_data.in32key(), &row_data);
        kv_mstringkeydata_.emplace(row_data.mstringkey(), &row_data);
        kv_muint32keydata_.emplace(row_data.muint32key(), &row_data);
        kv_min32keydata_.emplace(row_data.min32key(), &row_data);
    }
}

std::pair<const TestMultiKeyTable*, uint32_t> TestMultiKeyConfigurationTable::GetTable(const uint32_t tableId) {
    const auto it = kv_data_.find(tableId);
    if (it == kv_data_.end()) {
        LOG_ERROR << "TestMultiKey table not found for ID: " << tableId;
        return { nullptr, kInvalidTableId };
    }
    return { it->second, kSuccess };
}

std::pair<const TestMultiKeyTable*, uint32_t> TestMultiKeyConfigurationTable::GetTableWithoutErrorLogging(const uint32_t tableId) {
    const auto it = kv_data_.find(tableId);
    if (it == kv_data_.end()) {
        return { nullptr, kInvalidTableId };
    }
    return { it->second, kSuccess };
}
std::pair<const TestMultiKeyTable*, uint32_t> TestMultiKeyConfigurationTable::GetByStringkey(const std::string& tableId) const {
    const auto it = kv_stringkeydata_.find(tableId);
    if (it == kv_stringkeydata_.end()) {
        LOG_ERROR << "TestMultiKey table not found for ID: " << tableId;
        return { nullptr, kInvalidTableId };
    }
    return { it->second, kSuccess };
}
std::pair<const TestMultiKeyTable*, uint32_t> TestMultiKeyConfigurationTable::GetByUint32key(uint32_t tableId) const {
    const auto it = kv_uint32keydata_.find(tableId);
    if (it == kv_uint32keydata_.end()) {
        LOG_ERROR << "TestMultiKey table not found for ID: " << tableId;
        return { nullptr, kInvalidTableId };
    }
    return { it->second, kSuccess };
}
std::pair<const TestMultiKeyTable*, uint32_t> TestMultiKeyConfigurationTable::GetByIn32key(int32_t tableId) const {
    const auto it = kv_in32keydata_.find(tableId);
    if (it == kv_in32keydata_.end()) {
        LOG_ERROR << "TestMultiKey table not found for ID: " << tableId;
        return { nullptr, kInvalidTableId };
    }
    return { it->second, kSuccess };
}
std::pair<const TestMultiKeyTable*, uint32_t> TestMultiKeyConfigurationTable::GetByMstringkey(const std::string& tableId) const {
    const auto it = kv_mstringkeydata_.find(tableId);
    if (it == kv_mstringkeydata_.end()) {
        LOG_ERROR << "TestMultiKey table not found for ID: " << tableId;
        return { nullptr, kInvalidTableId };
    }
    return { it->second, kSuccess };
}
std::pair<const TestMultiKeyTable*, uint32_t> TestMultiKeyConfigurationTable::GetByMuint32key(uint32_t tableId) const {
    const auto it = kv_muint32keydata_.find(tableId);
    if (it == kv_muint32keydata_.end()) {
        LOG_ERROR << "TestMultiKey table not found for ID: " << tableId;
        return { nullptr, kInvalidTableId };
    }
    return { it->second, kSuccess };
}
std::pair<const TestMultiKeyTable*, uint32_t> TestMultiKeyConfigurationTable::GetByMin32key(int32_t tableId) const {
    const auto it = kv_min32keydata_.find(tableId);
    if (it == kv_min32keydata_.end()) {
        LOG_ERROR << "TestMultiKey table not found for ID: " << tableId;
        return { nullptr, kInvalidTableId };
    }
    return { it->second, kSuccess };
}