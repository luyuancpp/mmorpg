#include "google/protobuf/util/json_util.h"
#include "src/util/file2string.h"
#include "muduo/base/Logging.h"
#include "common_error_tip.pb.h"
#include "skill_config.h"

void SkillConfigurationTable::Load() {
    data_.Clear();
    const auto contents = File2String("config/generated/json/skill.json");
    if (const auto result = google::protobuf::util::JsonStringToMessage(contents.data(), &data_); !result.ok()) {
        LOG_FATAL << "Skill " << result.message().data();
    }

    for (int32_t i = 0; i < data_.data_size(); ++i) { 
        const auto& row_data = data_.data(i);
        kv_data_.emplace(row_data.id(), &row_data);


    }

    {
      expression_damage_.Init({
   "level"
     });
    }
}


std::pair<const SkillTable*, uint32_t> SkillConfigurationTable::GetTable(const uint32_t keyId) {
    const auto it = kv_data_.find(keyId);
    if (it == kv_data_.end()) {
        LOG_ERROR << "Skill table not found for ID: " << keyId;
        return { nullptr, kInvalidTableId };
    }
    return { it->second, kOK };
}

