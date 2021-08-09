#include "redis_client.h"

#include "muduo/base/CrossPlatformAdapterFunction.h"
#include "muduo/base/Logging.h"

#include "google/protobuf/message.h"

namespace common
{

void RedisClient::Connect(const std::string& redis_server_addr, int32_t port, int32_t sec, int32_t usec)
{
    struct timeval timeout = { sec, usec };
    context_ = std::shared_ptr<redisContext>
        (redisConnectWithTimeout(redis_server_addr.c_str(), port, timeout),
            redisFree);
}

void RedisClient::Save(const google::protobuf::Message& message)
{
    const auto * desc = message.GetDescriptor();
    Save(message, desc->full_name());
}

void RedisClient::Save(const google::protobuf::Message& message, GameGuid game_guid)
{
    const auto* desc = message.GetDescriptor();
    std::string key = desc->full_name() + std::to_string(game_guid);
    Save(message, key);
}

void RedisClient::Save(const google::protobuf::Message& message, const std::string& key)
{
    size_t key_len = key.length();
    MessageCachedArray message_cached_array(message.ByteSizeLong());
    if (message_cached_array.empty())
    {
        return;
    }
    message.SerializeWithCachedSizesToArray(message_cached_array.data());
    redisReply* reply = (redisReply*)redisCommand(context_.get(),
        "SET %b %b",
        key.c_str(),
        key_len,
        message_cached_array.data(),
        message_cached_array.size());
    freeReplyObject(reply);
}

void RedisClient::Load(google::protobuf::Message& message)
{
    const auto* desc = message.GetDescriptor();
    Load(message, desc->full_name());
}

void RedisClient::Load(google::protobuf::Message& message, GameGuid game_guid)
{
    const auto* desc = message.GetDescriptor();
    std::string key = desc->full_name() + std::to_string(game_guid);
    Load(message, key);
}

void RedisClient::Load(google::protobuf::Message& message, const std::string& key)
{
    std::string format = std::string("GET ") + key;
    redisReply* reply = (redisReply*)redisCommand(context_.get(),
        format.c_str());
    if (nullptr == reply)
    {
        return;
    }
    message.ParseFromArray(reply->str, static_cast<int32_t>(reply->len));
    freeReplyObject(reply);
}

void RedisClient::OnDisconnect()
{
    
}

}//namespace common


