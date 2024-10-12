#include "google/protobuf/util/json_util.h"
#include "src/util/file2string.h"
#include "muduo/base/Logging.h"
#include "common_error_tip.pb.h"
#include "scene_config.h"

void SceneConfigurationTable::Load() {
    data_.Clear();
    const auto contents = File2String("config/generated/json/scene.json");
    if (const auto result = google::protobuf::util::JsonStringToMessage(contents.data(), &data_); !result.ok()) {
        LOG_FATAL << "Scene " << result.message().data();
    }

    for (int32_t i = 0; i < data_.data_size(); ++i) { 
        const auto& row_data = data_.data(i);
        kv_data_.emplace(row_data.id(), &row_data);


    }

}


std::pair<const SceneTable*, uint32_t> SceneConfigurationTable::GetTable(const uint32_t keyid) {
    const auto it = kv_data_.find(keyid);
    if (it == kv_data_.end()) {
        LOG_ERROR << "Scene table not found for ID: " << keyid;
        return { nullptr, kInvalidTableId };
    }
    return { it->second, kOK };
}

