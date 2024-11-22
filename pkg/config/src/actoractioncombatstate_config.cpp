#include "google/protobuf/util/json_util.h"
#include "src/util/file2string.h"
#include "muduo/base/Logging.h"
#include "common_error_tip.pb.h"
#include "actoractioncombatstate_config.h"

void ActorActionCombatStateConfigurationTable::Load() {
    data_.Clear();
    const auto contents = File2String("config/generated/json/actoractioncombatstate.json");
    if (const auto result = google::protobuf::util::JsonStringToMessage(contents.data(), &data_); !result.ok()) {
        LOG_FATAL << "ActorActionCombatState " << result.message().data();
    }

    for (int32_t i = 0; i < data_.data_size(); ++i) { 
        const auto& row_data = data_.data(i);
        kv_data_.emplace(row_data.id(), &row_data);


    }

}


std::pair<const ActorActionCombatStateTable*, uint32_t> ActorActionCombatStateConfigurationTable::GetTable(const uint32_t keyId) {
    const auto it = kv_data_.find(keyId);
    if (it == kv_data_.end()) {
        LOG_ERROR << "ActorActionCombatState table not found for ID: " << keyId;
        return { nullptr, kInvalidTableId };
    }
    return { it->second, kSuccess };
}

