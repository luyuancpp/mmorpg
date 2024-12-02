#include "google/protobuf/util/json_util.h"
#include "src/util/file2string.h"
#include "common_error_tip.pb.h"
#include "globalvariable_config.h"

void GlobalVariableConfigurationTable::Load() {
    data_.Clear();
    const auto contents = File2String("config/generated/json/globalvariable.json");
    if (const auto result = google::protobuf::util::JsonStringToMessage(contents.data(), &data_); !result.ok()) {
        LOG_FATAL << "GlobalVariable " << result.message().data();
    }

    for (int32_t i = 0; i < data_.data_size(); ++i) { 
        const auto& row_data = data_.data(i);
        kv_data_.emplace(row_data.id(), &row_data);


    }

}


std::pair<const GlobalVariableTable*, uint32_t> GlobalVariableConfigurationTable::GetTable(const uint32_t keyId) {
    const auto it = kv_data_.find(keyId);
    if (it == kv_data_.end()) {
       LOG_ERROR << "GlobalVariable table not found for ID: " << keyId;
        return { nullptr, kInvalidTableId };
    }
    return { it->second, kSuccess };
}

