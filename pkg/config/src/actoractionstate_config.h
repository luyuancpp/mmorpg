#pragma once
#include <cstdint>
#include <memory>
#include <unordered_map>
#include "config_expression/config_expression.h"
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

inline std::pair<const ActorActionStateTable*, uint32_t> GetActorActionStateTable(const uint32_t keyId) { return ActorActionStateConfigurationTable::Instance().GetTable(keyId); }

inline const ActorActionStateTabledData& GetActorActionStateAllTable() { return ActorActionStateConfigurationTable::Instance().All(); }

#define FetchAndValidateActorActionStateTable(keyId) \
const auto [actorActionStateTable, fetchResult] = ActorActionStateConfigurationTable::Instance().GetTable(keyId); \
if (!(actorActionStateTable)) { return (fetchResult); }

#define FetchActorActionStateTableOrReturnVoid(keyId) \
const auto [actorActionStateTable, fetchResult] = ActorActionStateConfigurationTable::Instance().GetTable(keyId); \
if (!(actorActionStateTable)) { return ;}

#define FetchActorActionStateTableOrContinue(keyId) \
const auto [actorActionStateTable, fetchResult] = ActorActionStateConfigurationTable::Instance().GetTable(keyId); \
if (!(actorActionStateTable)) { continue; }

#define FetchActorActionStateTableOrReturnFalse(keyId) \
const auto [actorActionStateTable, fetchResult] = ActorActionStateConfigurationTable::Instance().GetTable(keyId); \
if (!(actorActionStateTable)) { return false; }