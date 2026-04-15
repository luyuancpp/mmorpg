#pragma once
#include <algorithm>
#include <cstdint>
#include <functional>
#include <random>
#include <unordered_map>
#include <vector>
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

    // ---- Has / Exists ----

    bool HasId(uint32_t id) const { return kv_data_.count(id) > 0; }

    // ---- Len / Count ----

    std::size_t Len() const { return kv_data_.size(); }

    // ---- Batch Lookup (IN) ----

    std::vector<const MessageLimiterTable*> GetByIds(const std::vector<uint32_t>& ids) const {
        std::vector<const MessageLimiterTable*> result;
        result.reserve(ids.size());
        for (auto id : ids) {
            if (auto it = kv_data_.find(id); it != kv_data_.end()) {
                result.push_back(it->second);
            }
        }
        return result;
    }

    // ---- Random ----

    const MessageLimiterTable* GetRandom() const {
        if (data_.data_size() == 0) return nullptr;
        thread_local std::mt19937 rng{std::random_device{}()};
        std::uniform_int_distribution<int> dist(0, data_.data_size() - 1);
        return &data_.data(dist(rng));
    }

    // ---- Filter / FindFirst ----

    std::vector<const MessageLimiterTable*> Filter(const std::function<bool(const MessageLimiterTable&)>& pred) const {
        std::vector<const MessageLimiterTable*> result;
        for (int i = 0; i < data_.data_size(); ++i) {
            if (pred(data_.data(i))) {
                result.push_back(&data_.data(i));
            }
        }
        return result;
    }

    const MessageLimiterTable* FindFirst(const std::function<bool(const MessageLimiterTable&)>& pred) const {
        for (int i = 0; i < data_.data_size(); ++i) {
            if (pred(data_.data(i))) {
                return &data_.data(i);
            }
        }
        return nullptr;
    }

    // ---- Composite Key ----

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
