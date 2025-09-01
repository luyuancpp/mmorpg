#include "redis_client.h"

#include "google/protobuf/message.h"

#include "core/utils/defer/defer.h"

void SyncRedisContext_Deleter::operator()(redisContext* res)
{
    redisFree(res);
}

void MessageSyncRedisClient::Connect(const std::string& redis_server_addr, int32_t port, int32_t sec, int32_t usec)
{
    timeval timeout = { sec, usec };
    context_.reset(redisConnectWithTimeout(redis_server_addr.c_str(), port, timeout));

    if (nullptr == context_)
    {
        LOG_FATAL << "Connect Redis " << redis_server_addr << ":" << port;
    }
    else if (context_->err != REDIS_OK)
    {
        LOG_FATAL << "Connect Redis " << redis_server_addr << ":" << port << context_->errstr;
    }
}

void MessageSyncRedisClient::Save(const google::protobuf::Message& message)
{
    const auto * desc = message.GetDescriptor();
    Save(message, std::string(desc->full_name()));
}

void MessageSyncRedisClient::Save(const google::protobuf::Message& message, Guid guid)
{
    const auto* desc = message.GetDescriptor();

    if (kInvalidGuid == guid)
    {
        LOG_ERROR << "Message Save To Redis Game Guid Key Empty : " << desc->full_name().data();
        return;
    }

    std::string key = std::string(desc->full_name()) + std::to_string(guid);
    Save(message, key);
}

void MessageSyncRedisClient::Save(const google::protobuf::Message& message, const std::string& key)
{
	if (key.empty())
	{
		const auto* desc = message.GetDescriptor();
		LOG_ERROR << "Message Save To Redis Key Empty : " << desc->full_name().data();
		return;
	}

	MessageCachedArray message_cached_array(message.ByteSizeLong());
	message.SerializeWithCachedSizesToArray(message_cached_array.data());

	redisReply* reply = (redisReply*)redisCommand(context_.get(),
		"EVAL %s 1 %b %b",
		kSaveAndMarkLuaScript,
		key.c_str(),
		(size_t)key.length(),
		message_cached_array.data(),
		message_cached_array.size()
	);

	defer(freeReplyObject(reply));

	if (!reply || reply->type == REDIS_REPLY_ERROR)
	{
		LOG_ERROR << "Redis EVAL failed for key: " << key;
		return;
	}
}


void MessageSyncRedisClient::Load(google::protobuf::Message& message)
{
    const auto* desc = message.GetDescriptor();
    Load(message, desc->full_name().data());
}

void MessageSyncRedisClient::Load(google::protobuf::Message& message, Guid guid)
{
    const auto* desc = message.GetDescriptor();
    std::string key = std::string(desc->full_name()) + std::to_string(guid);
    Load(message, key);
}

void MessageSyncRedisClient::Load(google::protobuf::Message& message, const std::string& key)
{
    if (key.empty())
    {
        const auto* desc = message.GetDescriptor();
        LOG_ERROR << "Message Load From Redis Key Empty : " << desc->full_name().data();
        return;
    }

    std::string format = std::string("GET ") + key;
    redisReply* reply = static_cast<redisReply*>(redisCommand(context_.get(),
                                                              format.c_str()));
    defer(freeReplyObject(reply));
    if (nullptr == reply)
    {
        return;
    }

    message.ParsePartialFromArray(reply->str, static_cast<int32_t>(reply->len));
}

void MessageSyncRedisClient::OnDisconnect()
{
    
}

