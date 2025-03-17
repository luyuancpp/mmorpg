#pragma once

#include <string>
#include <memory>
#include <vector>


#include "muduo/base/Logging.h"
#include "muduo/contrib/hiredis/Hiredis.h"

#include "deps/hiredis/hiredis.h"

#include "type_define/type_define.h"

namespace google
{
    namespace protobuf
    {
        class Message;
    }//namespace protobuf
}// namespace google


using MessageCachedArray = std::vector<uint8_t>;

class SyncRedisContext_Deleter
{
public:
    void operator()(redisContext* res);
};

class MessageSyncRedisClient
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

using PbSyncRedisClientPtr = std::shared_ptr<MessageSyncRedisClient>;

template <class MessageKey, class MessageValue>
class MessageAsyncClient
{
public:
    using MessageValuePtr = std::shared_ptr<MessageValue>;

    struct Element
    {
        using MessageValuePtr = std::shared_ptr<MessageValue>;
        MessageKey message_key;
        std::string redis_key;
        MessageValuePtr message_value;
    };

    using ElementPtr = std::shared_ptr<Element>;
    using LoadingQueue = std::unordered_map<std::string, ElementPtr>;
    using EventCallback = std::function<void(MessageKey, MessageValue&)>;

    explicit MessageAsyncClient(hiredis::Hiredis& hiredis)
        : hiredis_(hiredis)
    {
    }

    inline const std::string full_name() const { return MessageValue::GetDescriptor()->full_name().data(); }
    void SetSaveCallback(const EventCallback& cb) { save_callback_ = cb; }
    void SetLoadCallback(const EventCallback& cb) { load_callback_ = cb; }

    void Save(const MessageValuePtr& message, const MessageKey& key)
    {
        ElementPtr element = std::make_shared<Element>();
        element->message_key = key;
        element->redis_key = full_name() + std::to_string(key);
        element->message_value = message;
        MessageCachedArray message_cached_array(message->ByteSizeLong());
        message->SerializeWithCachedSizesToArray(message_cached_array.data());
        hiredis_.command(std::bind(&MessageAsyncClient::OnSaved, this, std::placeholders::_1, std::placeholders::_2, element),
            "SET %b %b", element->redis_key.c_str(), element->redis_key.length(),
            message_cached_array.data(),
            message_cached_array.size());
    }

    void AsyncLoad(const MessageKey& key)
    {
        const std::string redis_key = full_name() + std::to_string(key);
        if (loading_queue_.find(redis_key) != loading_queue_.end())
        {
            return;
        }
        ElementPtr element = std::make_shared<Element>();
        element->message_key = key;
        element->redis_key = std::move(redis_key);
        loading_queue_.emplace(element->redis_key, element);
        const std::string format = std::string("GET ") + element->redis_key;
        hiredis_.command(std::bind(&MessageAsyncClient::OnLoaded, this, std::placeholders::_1, std::placeholders::_2, element),
            format.c_str());
    }

    MessageValuePtr CreateMessage() { return std::make_shared<MessageValue>(); }
private:
    void OnSaved(hiredis::Hiredis* c, redisReply* reply, ElementPtr element)
    {
        if (!save_callback_)
        {
            return;
        }
        save_callback_(element->message_key, *element->message_value);
    }

    void OnLoaded(hiredis::Hiredis* c, redisReply* reply, ElementPtr element)
    {
        element->message_value = CreateMessage();
        element->message_value->ParseFromArray(reply->str, static_cast<int32_t>(reply->len));
        loading_queue_.erase(element->redis_key);
        if (!load_callback_)
        {
            return;
        }
        load_callback_(element->message_key, *element->message_value);
    }

    hiredis::Hiredis& hiredis_;
    LoadingQueue loading_queue_;
    EventCallback save_callback_;
    EventCallback load_callback_;
};

