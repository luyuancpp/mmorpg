#pragma once
#include <cstdint>
#include <unordered_map>
#include "config_expression/config_expression.h"
#include "muduo/base/Logging.h"
#include "messagelimiter_config.pb.h"

class MessageLimiterConfigurationTable {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const MessageLimiterTable*>;

    static MessageLimiterConfigurationTable& Instance() {
        static MessageLimiterConfigurationTable instance;
        return instance;
    }

    const MessageLimiterTabledData& All() const { return data_; }
    std::pair<const MessageLimiterTable*, uint32_t> GetTable(uint32_t tableId);
    std::pair<const MessageLimiterTable*, uint32_t> GetTableWithoutErrorLogging(uint32_t tableId);
    const KeyValueDataType& KeyValueData() const { return kv_data_; }
    void Load();
private:
    MessageLimiterTabledData data_;
    KeyValueDataType kv_data_;
};

inline const MessageLimiterTabledData& GetMessageLimiterAllTable() {
    return MessageLimiterConfigurationTable::Instance().All();
}

#define FetchAndValidateMessageLimiterTable(tableId) \
    const auto [messagelimiterTable, fetchResult] = MessageLimiterConfigurationTable::Instance().GetTable(tableId); \
    do { if (!( messagelimiterTable )) { LOG_ERROR << "MessageLimiter table not found for ID: " << tableId; return fetchResult; } } while(0)

#define FetchAndValidateCustomMessageLimiterTable(prefix, tableId) \
    const auto [prefix##MessageLimiterTable, prefix##fetchResult] = MessageLimiterConfigurationTable::Instance().GetTable(tableId); \
    do { if (!(prefix##MessageLimiterTable)) { LOG_ERROR << "MessageLimiter table not found for ID: " << tableId; return prefix##fetchResult; } } while(0)

#define FetchMessageLimiterTableOrReturnCustom(tableId, customReturnValue) \
    const auto [messagelimiterTable, fetchResult] = MessageLimiterConfigurationTable::Instance().GetTable(tableId); \
    do { if (!( messagelimiterTable )) { LOG_ERROR << "MessageLimiter table not found for ID: " << tableId; return customReturnValue; } } while(0)

#define FetchMessageLimiterTableOrReturnVoid(tableId) \
    const auto [messagelimiterTable, fetchResult] = MessageLimiterConfigurationTable::Instance().GetTable(tableId); \
    do { if (!( messagelimiterTable )) { LOG_ERROR << "MessageLimiter table not found for ID: " << tableId; return; } } while(0)

#define FetchMessageLimiterTableOrContinue(tableId) \
    const auto [messagelimiterTable, fetchResult] = MessageLimiterConfigurationTable::Instance().GetTable(tableId); \
    do { if (!( messagelimiterTable )) { LOG_ERROR << "MessageLimiter table not found for ID: " << tableId; continue; } } while(0)

#define FetchMessageLimiterTableOrReturnFalse(tableId) \
    const auto [messagelimiterTable, fetchResult] = MessageLimiterConfigurationTable::Instance().GetTable(tableId); \
    do { if (!( messagelimiterTable )) { LOG_ERROR << "MessageLimiter table not found for ID: " << tableId; return false; } } while(0)