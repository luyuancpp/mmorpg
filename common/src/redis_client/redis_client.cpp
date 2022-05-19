#include "redis_client.h"

#include "google/protobuf/message.h"

void PbSyncRedisClient::Connect(const std::string& redis_server_addr, int32_t port, int32_t sec, int32_t usec)
{
    struct timeval timeout = { sec, usec };
    context_.reset(redisConnectWithTimeout(redis_server_addr.c_str(), port, timeout));
    if (nullptr == context_)
    {
        LOG_FATAL << "Conect Redis " << redis_server_addr << ":" << port;
    }
    else if (context_->err != REDIS_OK)
    {
        LOG_FATAL << "Conect Redis " << redis_server_addr << ":" << port << context_->errstr;
    }
}

void PbSyncRedisClient::Save(const google::protobuf::Message& message)
{
    const auto * desc = message.GetDescriptor();
    Save(message, desc->full_name());
}

void PbSyncRedisClient::Save(const google::protobuf::Message& message, Guid guid)
{
    const auto* desc = message.GetDescriptor();
    if (kInvalidGuid == guid)
    {
        LOG_ERROR << "Message Save To Redis Gameguid Key Empty : " << desc->full_name();
        return;
    }
    std::string key = desc->full_name() + std::to_string(guid);
    Save(message, key);
}

void PbSyncRedisClient::Save(const google::protobuf::Message& message, const std::string& key)
{
    if (key.empty())
    {
        const auto* desc = message.GetDescriptor();
        LOG_ERROR << "Message Save To Redis Key Empty : " << desc->full_name();
        return;
    }
    size_t key_len = key.length();
    MessageCachedArray message_cached_array(message.ByteSizeLong());
    if (message_cached_array.empty())
    {
        const auto* desc = message.GetDescriptor();
        LOG_ERROR << "Message Save To Redis Message Empty : " << desc->full_name();
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

void PbSyncRedisClient::Load(google::protobuf::Message& message)
{
    const auto* desc = message.GetDescriptor();
    Load(message, desc->full_name());
}

void PbSyncRedisClient::Load(google::protobuf::Message& message, Guid guid)
{
    const auto* desc = message.GetDescriptor();
    std::string key = desc->full_name() + std::to_string(guid);
    Load(message, key);
}

void PbSyncRedisClient::Load(google::protobuf::Message& message, const std::string& key)
{
    if (key.empty())
    {
        const auto* desc = message.GetDescriptor();
        LOG_ERROR << "Message Load From Redis Key Empty : " << desc->full_name();
        return;
    }
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

void PbSyncRedisClient::OnDisconnect()
{
    
}

