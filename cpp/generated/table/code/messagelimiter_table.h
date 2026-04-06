#pragma once
#include <cstdint>
#include <functional>
#include <unordered_map>
#include "table_expression.h"
#include "muduo/base/Logging.h"
#include "table/proto/messagelimiter_table.pb.h"

class MessageLimiterTableManager {
public:
    using KeyValueDataType = std::unordered_map<uint32_t, const MessageLimiterTable*>;
    using LoadSuccessCallback = std::function<void()>;

    static MessageLimiterTableManager& Instance() {
        static MessageLimiterTableManager instance;
        return instance;
    }

    const MessageLimiterTableData& All() const { return data_; }

    std::pair<const MessageLimiterTable*, uint32_t> GetTable(uint32_t tableId);
    std::pair<const MessageLimiterTable*, uint32_t> GetTableWithoutErrorLogging(uint32_t tableId);
    const KeyValueDataType& KeyValueData() const { return kv_data_; }

    void Load();

    void SetLoadSuccessCallback(const LoadSuccessCallback& callback) {
        loadSuccessCallback_ = callback;
    }

    void LoadSuccess() { if (loadSuccessCallback_) { loadSuccessCallback_(); } }

private:
    LoadSuccessCallback loadSuccessCallback_;
    MessageLimiterTableData data_;
    KeyValueDataType kv_data_;
};

inline const MessageLimiterTableData& GetMessageLimiterAllTable() {
    return MessageLimiterTableManager::Instance().All();
}

#define FetchAndValidateMessageLimiterTable(tableId) \
    const auto [messageLimiterTable, fetchResult] = MessageLimiterTableManager::Instance().GetTable(tableId); \
    do { if (!(messageLimiterTable)) { LOG_ERROR << "MessageLimiter table not found for ID: " << tableId; return fetchResult; } } while(0)

#define FetchAndValidateCustomMessageLimiterTable(prefix, tableId) \
    const auto [prefix##MessageLimiterTable, prefix##fetchResult] = MessageLimiterTableManager::Instance().GetTable(tableId); \
    do { if (!(prefix##MessageLimiterTable)) { LOG_ERROR << "MessageLimiter table not found for ID: " << tableId; return prefix##fetchResult; } } while(0)

#define FetchMessageLimiterTableOrReturnCustom(tableId, customReturnValue) \
    const auto [messageLimiterTable, fetchResult] = MessageLimiterTableManager::Instance().GetTable(tableId); \
    do { if (!(messageLimiterTable)) { LOG_ERROR << "MessageLimiter table not found for ID: " << tableId; return customReturnValue; } } while(0)

#define FetchMessageLimiterTableOrReturnVoid(tableId) \
    const auto [messageLimiterTable, fetchResult] = MessageLimiterTableManager::Instance().GetTable(tableId); \
    do { if (!(messageLimiterTable)) { LOG_ERROR << "MessageLimiter table not found for ID: " << tableId; return; } } while(0)

#define FetchMessageLimiterTableOrContinue(tableId) \
    const auto [messageLimiterTable, fetchResult] = MessageLimiterTableManager::Instance().GetTable(tableId); \
    do { if (!(messageLimiterTable)) { LOG_ERROR << "MessageLimiter table not found for ID: " << tableId; continue; } } while(0)

#define FetchMessageLimiterTableOrReturnFalse(tableId) \
    const auto [messageLimiterTable, fetchResult] = MessageLimiterTableManager::Instance().GetTable(tableId); \
    do { if (!(messageLimiterTable)) { LOG_ERROR << "MessageLimiter table not found for ID: " << tableId; return false; } } while(0)
