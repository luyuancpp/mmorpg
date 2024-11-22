#pragma once
#include <cstdint>
#include <memory>
#include <unordered_map>
#include "config_expression/config_expression.h"
#include "actoractioncombatstate_config.pb.h"


class ActorActionCombatStateConfigurationTable {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const ActorActionCombatStateTable*>;
    static ActorActionCombatStateConfigurationTable& Instance() { static ActorActionCombatStateConfigurationTable instance; return instance; }
    const ActorActionCombatStateTabledData& All() const { return data_; }
    std::pair<const ActorActionCombatStateTable*, uint32_t> GetTable(uint32_t keyId);
    const KeyValueDataType& KeyValueData() const { return kv_data_; }
    void Load();


private:
    ActorActionCombatStateTabledData data_;
    KeyValueDataType kv_data_;

};

inline std::pair<const ActorActionCombatStateTable*, uint32_t> GetActorActionCombatStateTable(const uint32_t keyId) { return ActorActionCombatStateConfigurationTable::Instance().GetTable(keyId); }

inline const ActorActionCombatStateTabledData& GetActorActionCombatStateAllTable() { return ActorActionCombatStateConfigurationTable::Instance().All(); }

#define FetchAndValidateActorActionCombatStateTable(keyId) \
const auto [actorActionCombatStateTable, fetchResult] = ActorActionCombatStateConfigurationTable::Instance().GetTable(keyId); \
if (!(actorActionCombatStateTable)) { return (fetchResult); }

#define FetchActorActionCombatStateTableOrReturnVoid(keyId) \
const auto [actorActionCombatStateTable, fetchResult] = ActorActionCombatStateConfigurationTable::Instance().GetTable(keyId); \
do {if (!(actorActionCombatStateTable)) { return ;}} while (0)

#define FetchActorActionCombatStateTableOrContinue(keyId) \
const auto [actorActionCombatStateTable, fetchResult] = ActorActionCombatStateConfigurationTable::Instance().GetTable(keyId); \
do { if (!(actorActionCombatStateTable)) { continue; }} while (0)

#define FetchActorActionCombatStateTableOrReturnFalse(keyId) \
const auto [actorActionCombatStateTable, fetchResult] = ActorActionCombatStateConfigurationTable::Instance().GetTable(keyId); \
do {if (!(actorActionCombatStateTable)) { return false; }} while (0)