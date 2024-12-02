#pragma once
#include <cstdint>
#include <memory>
#include <unordered_map>
#include "config_expression/config_expression.h"
#include "muduo/base/Logging.h"
#include "actoractionstate_config.pb.h"


class ActorActionStateConfigurationTable {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const ActorActionStateTable*>;
    static ActorActionStateConfigurationTable& Instance() { static ActorActionStateConfigurationTable instance; return instance; }
    const ActorActionStateTabledData& All() const { return data_; }
    std::pair<const ActorActionStateTable*, uint32_t> GetTable(uint32_t keyId);
    const KeyValueDataType& KeyValueData() const { return kv_data_; }
    void Load();


private:
    ActorActionStateTabledData data_;
    KeyValueDataType kv_data_;

};

inline const ActorActionStateTabledData& GetActorActionStateAllTable() { return ActorActionStateConfigurationTable::Instance().All(); }

#define FetchAndValidateActorActionStateTable(keyId) \
const auto [actorActionStateTable, fetchResult] = ActorActionStateConfigurationTable::Instance().GetTable(keyId); \
do {if (!(actorActionStateTable)) { LOG_ERROR << "ActorActionState table not found for ID: " << keyId;return (fetchResult); }} while (0)

#define FetchAndValidateCustomActorActionStateTable(prefix, keyId) \
const auto [##prefix##ActorActionStateTable, prefix##fetchResult] = ActorActionStateConfigurationTable::Instance().GetTable(keyId); \
do {if (!(##prefix##ActorActionStateTable)) { LOG_ERROR << "ActorActionState table not found for ID: " << keyId;return (prefix##fetchResult); }} while (0)

#define FetchActorActionStateTableOrReturnCustom(keyId, customReturnValue) \
const auto [actorActionStateTable, fetchResult] = ActorActionStateConfigurationTable::Instance().GetTable(keyId); \
do {if (!(actorActionStateTable)) { LOG_ERROR << "ActorActionState table not found for ID: " << keyId;return (customReturnValue); }} while (0)

#define FetchActorActionStateTableOrReturnVoid(keyId) \
const auto [actorActionStateTable, fetchResult] = ActorActionStateConfigurationTable::Instance().GetTable(keyId); \
do {if (!(actorActionStateTable)) { LOG_ERROR << "ActorActionState table not found for ID: " << keyId;return ;}} while (0)

#define FetchActorActionStateTableOrContinue(keyId) \
const auto [actorActionStateTable, fetchResult] = ActorActionStateConfigurationTable::Instance().GetTable(keyId); \
do { if (!(actorActionStateTable)) { LOG_ERROR << "ActorActionState table not found for ID: " << keyId;continue; }} while (0)

#define FetchActorActionStateTableOrReturnFalse(keyId) \
const auto [actorActionStateTable, fetchResult] = ActorActionStateConfigurationTable::Instance().GetTable(keyId); \
do {if (!(actorActionStateTable)) { LOG_ERROR << "ActorActionState table not found for ID: " << keyId;return false; }} while (0)