#include "google/protobuf/util/json_util.h"
#include "src/util/file2string.h"
#include "muduo/base/Logging.h"
#include "common_error_tip.pb.h"
#include "testmultikey_config.h"

void TestMultiKeyConfigurationTable::Load() {
    data_.Clear();
    const auto contents = File2String("config/generated/json/testmultikey.json");
    if (const auto result = google::protobuf::util::JsonStringToMessage(contents.data(), &data_); !result.ok()) {
        LOG_FATAL << "TestMultiKey " << result.message().data();
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


std::pair<const TestMultiKeyTable*, uint32_t> TestMultiKeyConfigurationTable::GetTable(uint32_t keyid) {
    const auto it = kv_data_.find(keyid);
    if (it == kv_data_.end()) {
        LOG_ERROR << "TestMultiKey table not found for ID: " << keyid;
        return { nullptr, kInvalidTableId };
    }
    return { it->second, kOK };
}


std::pair<const TestMultiKeyTable*, uint32_t> TestMultiKeyConfigurationTable::GetByStringkey(const std::string& keyid) const {
    const auto it = kv_stringkeydata_.find(keyid);
    if (it == kv_stringkeydata_.end()) {
        LOG_ERROR << "TestMultiKey table not found for ID: " << keyid;
        return { nullptr, kInvalidTableId };
    }
    return { it->second, kOK };
}

std::pair<const TestMultiKeyTable*, uint32_t> TestMultiKeyConfigurationTable::GetByUint32Key(uint32_t keyid) const {
    const auto it = kv_uint32keydata_.find(keyid);
    if (it == kv_uint32keydata_.end()) {
        LOG_ERROR << "TestMultiKey table not found for ID: " << keyid;
        return { nullptr, kInvalidTableId };
    }
    return { it->second, kOK };
}

std::pair<const TestMultiKeyTable*, uint32_t> TestMultiKeyConfigurationTable::GetByIn32Key(int32_t keyid) const {
    const auto it = kv_in32keydata_.find(keyid);
    if (it == kv_in32keydata_.end()) {
        LOG_ERROR << "TestMultiKey table not found for ID: " << keyid;
        return { nullptr, kInvalidTableId };
    }
    return { it->second, kOK };
}

std::pair<const TestMultiKeyTable*, uint32_t> TestMultiKeyConfigurationTable::GetByMstringkey(const std::string& keyid) const {
    const auto it = kv_mstringkeydata_.find(keyid);
    if (it == kv_mstringkeydata_.end()) {
        LOG_ERROR << "TestMultiKey table not found for ID: " << keyid;
        return { nullptr, kInvalidTableId };
    }
    return { it->second, kOK };
}

std::pair<const TestMultiKeyTable*, uint32_t> TestMultiKeyConfigurationTable::GetByMuint32Key(uint32_t keyid) const {
    const auto it = kv_muint32keydata_.find(keyid);
    if (it == kv_muint32keydata_.end()) {
        LOG_ERROR << "TestMultiKey table not found for ID: " << keyid;
        return { nullptr, kInvalidTableId };
    }
    return { it->second, kOK };
}

std::pair<const TestMultiKeyTable*, uint32_t> TestMultiKeyConfigurationTable::GetByMin32Key(int32_t keyid) const {
    const auto it = kv_min32keydata_.find(keyid);
    if (it == kv_min32keydata_.end()) {
        LOG_ERROR << "TestMultiKey table not found for ID: " << keyid;
        return { nullptr, kInvalidTableId };
    }
    return { it->second, kOK };
}
