#pragma once
#include <cstdint>
#include <unordered_map>
#include "config_expression/config_expression.h"
#include "muduo/base/Logging.h"
#include "class_config.pb.h"

class ClassConfigurationTable {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const ClassTable*>;

    static ClassConfigurationTable& Instance() {
        static ClassConfigurationTable instance;
        return instance;
    }

    const ClassTabledData& All() const { return data_; }
    std::pair<const ClassTable*, uint32_t> GetTable(uint32_t tableId);
    std::pair<const ClassTable*, uint32_t> GetTableWithoutErrorLogging(uint32_t tableId);
    const KeyValueDataType& KeyValueData() const { return kv_data_; }
    void Load();
private:
    ClassTabledData data_;
    KeyValueDataType kv_data_;
};

inline const ClassTabledData& GetClassAllTable() {
    return ClassConfigurationTable::Instance().All();
}

#define FetchAndValidateClassTable(tableId) \
    const auto [classTable, fetchResult] = ClassConfigurationTable::Instance().GetTable(tableId); \
    do { if (!( classTable )) { LOG_ERROR << "Class table not found for ID: " << tableId; return fetchResult; } } while(0)

#define FetchAndValidateCustomClassTable(prefix, tableId) \
    const auto [prefix##ClassTable, prefix##fetchResult] = ClassConfigurationTable::Instance().GetTable(tableId); \
    do { if (!(prefix##ClassTable)) { LOG_ERROR << "Class table not found for ID: " << tableId; return prefix##fetchResult; } } while(0)

#define FetchClassTableOrReturnCustom(tableId, customReturnValue) \
    const auto [classTable, fetchResult] = ClassConfigurationTable::Instance().GetTable(tableId); \
    do { if (!( classTable )) { LOG_ERROR << "Class table not found for ID: " << tableId; return customReturnValue; } } while(0)

#define FetchClassTableOrReturnVoid(tableId) \
    const auto [classTable, fetchResult] = ClassConfigurationTable::Instance().GetTable(tableId); \
    do { if (!( classTable )) { LOG_ERROR << "Class table not found for ID: " << tableId; return; } } while(0)

#define FetchClassTableOrContinue(tableId) \
    const auto [classTable, fetchResult] = ClassConfigurationTable::Instance().GetTable(tableId); \
    do { if (!( classTable )) { LOG_ERROR << "Class table not found for ID: " << tableId; continue; } } while(0)

#define FetchClassTableOrReturnFalse(tableId) \
    const auto [classTable, fetchResult] = ClassConfigurationTable::Instance().GetTable(tableId); \
    do { if (!( classTable )) { LOG_ERROR << "Class table not found for ID: " << tableId; return false; } } while(0)