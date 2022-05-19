#pragma once

#include <string>
#include <memory>
#include <vector>

#include "muduo/base/CrossPlatformAdapterFunction.h"
#include "muduo/base/Logging.h"
#include "muduo/contrib/hiredis/Hiredis.h"

#include "deps/hiredis/hiredis.h"

#include "src/common_type/common_type.h"

namespace google
{
    namespace protobuf
    {
        class Message;
    }//namespace protobuf
}// namespace google


using MessageCachedArray = std::vector<uint8_t>;

class SyncRedisContext_Deleter {
public:
    void operator()(redisContext* res) { redisFree(res); }
};

class PbSyncRedisClient
{
public:
    using ContextPtr = std::unique_ptr<redisContext, SyncRedisContext_Deleter>;
    void Connect(const std::string& redis_server_addr, int32_t port, int32_t sec, int32_t usec);

    void Save(const google::protobuf::Message& message);
    void Save(const google::protobuf::Message& message, Guid guid);
    void Save(const google::protobuf::Message& message, const std::string& key);

    void Load(google::protobuf::Message& message);
    void Load(google::protobuf::Message& message, Guid guid);
    void Load(google::protobuf::Message& message, const std::string& key);
private:
    void OnDisconnect();
   
    ContextPtr context_;
};

using PbSyncRedisClientPtr = std::shared_ptr<PbSyncRedisClient>;


template<class PbType>
class PbGuidAsyncClient
{
public:
    using PbTypPtr = std::shared_ptr<PbType>;
    using LoadingQueue = std::unordered_map<std::string, PbTypPtr>;
    using CommandCallback = std::function<PbTypPtr&>;
    PbGuidAsyncClient(hiredis::Hiredis& hiredis)
        :hiredis_(hiredis)
    {
    }
    inline const std::string& full_name() const { return PbType::GetDescriptor()->full_name();}
    void SetSaveCallback(const CommandCallback& cb) { save_callback_ = cb; }
    void SetLoadCallback(const CommandCallback& cb) { load_callback_ = cb; }

    void Save(PbType& message) 
    {
        const std::string key = full_name() + std::to_string(message->guid());
        if (key.empty())
        {
            LOG_ERROR << "Message Save To Redis Key Empty : " << full_name();
            return;
        }
        MessageCachedArray message_cached_array(message->ByteSizeLong());
        if (message_cached_array.empty())
        {
            LOG_ERROR << "Message Save To Redis Message Empty : " << full_name();
            return;
        }
        message->SerializeWithCachedSizesToArray(message_cached_array.data());
        hiredis.command(std::bind(SavePbCallback, std::placeholders::_1, std::placeholders::_2, message), "SET %b %b", key.c_str(), key.length(),
            message_cached_array.data(),
            message_cached_array.size());
    }

    void Load(Guid guid)
    {
        const std::string key = full_name() + std::to_string(guid);
        if (loading_queue_.find(key) != loading_queue_.end())
        {
            return;
        }
        PbTypPtr message;
        loading_queue_.emplace(key, message);
        std::string format = std::string("GET ") + key;
        hiredis.command(std::bind(LoadPbCallback, std::placeholders::_1, std::placeholders::_2, message), format.c_str());
    }
private:
    void SavePbCallback(hiredis::Hiredis* c, redisReply* reply, PbTypPtr message)
    {
        if (!save_callback_)
        {
            return;
        }
        save_callback_(message);
    }

    void LoadPbCallback(hiredis::Hiredis* c, redisReply* reply, PbTypPtr message)
    {
        const auto* desc = message->GetDescriptor();
        message->ParseFromArray(reply->str, static_cast<int32_t>(reply->len));
        loading_queue_.erase(full_name() + std::to_string(message->guid()));
        if (!load_callback_)
        {
            return;
        }        
        load_callback_(message);
    }

    hiredis::Hiredis& hiredis_;
    LoadingQueue loading_queue_;
    CommandCallback save_callback_;
    CommandCallback load_callback_;
};

