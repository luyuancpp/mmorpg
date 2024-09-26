#include "google/protobuf/util/json_util.h"
#include "src/util/file2string.h"
#include "muduo/base/Logging.h"
#include "common_error_tip.pb.h"
#include "ability_config.h"

void AbilityConfigurationTable::Load()
{
    data_.Clear();
    const auto contents = File2String("config/generated/json/ability.json");
    if (const auto result = google::protobuf::util::JsonStringToMessage(contents.data(), &data_);
        !result.ok()){
        LOG_FATAL << "Ability " << result.message().data();
    }

    for (int32_t i = 0; i < data_.data_size(); ++i){
        const auto& row_data = data_.data(i);
        key_data_.emplace(row_data.id(), &row_data);
    }
}

const std::pair< AbilityConfigurationTable::row_type, uint32_t> AbilityConfigurationTable::GetTable(uint32_t keyid)
{
    const auto it = key_data_.find(keyid);
    if (it == key_data_.end()){
      LOG_ERROR << "Ability table not found for ID: " << keyid; 
      return { nullptr, kInvalidTableId }; 
    }
    return { it->second, kOK};
}
