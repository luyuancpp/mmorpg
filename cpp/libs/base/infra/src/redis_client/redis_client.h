#pragma once

#include <string>
#include <memory>
#include <vector>
#include <any>

#include "muduo/base/Logging.h"
#include "muduo/contrib/hiredis/Hiredis.h"

#include "deps/hiredis/hiredis.h"

#include "type_define/type_define.h"

static constexpr const char* kSaveAndMarkLuaScript = R"(
    redis.call('SET', KEYS[1], ARGV[1])
    redis.call('SADD', 'dirty_keys_set', KEYS[1])
    return 1
)";


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
		MessageKey message_key;
		std::string redis_key;
		MessageValuePtr message_value;
		std::any extra_data;  // 用于存储附加信息
	};

	using ElementPtr = std::shared_ptr<Element>;
	using LoadingQueue = std::unordered_map<std::string, ElementPtr>;
	using EventCallback = std::function<void(MessageKey, MessageValue&)>;
	using EventCallbackWithExtra = std::function<void(MessageKey, MessageValue&, const std::any&)>;

	explicit MessageAsyncClient(hiredis::Hiredis& hiredis)
		: hiredis_(hiredis)
	{
	}

	inline const std::string full_name() const { return MessageValue::GetDescriptor()->full_name(); }

	void SetSaveCallback(const EventCallback& cb) { save_callback_ = cb; }
	void SetLoadCallback(const EventCallback& cb) { load_callback_ = cb; }
	void SetLoadCallbackWithExtra(const EventCallbackWithExtra& cb) { load_callback_with_extra_ = cb; }


	void Save(const MessageValuePtr& message, const MessageKey& key)
	{
		ElementPtr element = std::make_shared<Element>();
		element->message_key = key;
		element->redis_key = full_name() + ":" + std::to_string(key);
		element->message_value = message;

		const size_t size = message->ByteSizeLong();
		MessageCachedArray message_cached_array(size);
		if (!message->SerializeToArray(message_cached_array.data(), static_cast<int>(size)))
		{
			LOG_ERROR << "SerializeToArray failed for key " << key;
			return;
		}

		// 调用 Lua 脚本原子执行 SET 和 SADD
		hiredis_.command(std::bind(&MessageAsyncClient::OnSaved, this, std::placeholders::_1, std::placeholders::_2, element),
			"EVAL %s 1 %b %b",
			kSaveAndMarkLuaScript,
			element->redis_key.c_str(), element->redis_key.length(),
			message_cached_array.data(), message_cached_array.size()
		);
	}

	void AsyncLoad(const MessageKey& key, std::any extra_data = {})
	{
		std::string redis_key = full_name() + ":" + std::to_string(key);
		if (loading_queue_.find(redis_key) != loading_queue_.end())
		{
			return; // 已在加载中
		}

		ElementPtr element = std::make_shared<Element>();
		element->message_key = key;
		element->redis_key = redis_key;
		element->extra_data = std::move(extra_data);

		loading_queue_.emplace(redis_key, element);

		const std::string format = "GET " + redis_key;
		hiredis_.command(std::bind(&MessageAsyncClient::OnLoaded, this, std::placeholders::_1, std::placeholders::_2, element),
			format.c_str());
	}

	bool UpdateExtraData(const MessageKey& key, std::any new_extra_data)
	{
		std::string redis_key = full_name() + ":" + std::to_string(key);
		auto it = loading_queue_.find(redis_key);
		if (it == loading_queue_.end())
		{
			return false; // 未找到，说明不在加载中
		}

		it->second->extra_data = std::move(new_extra_data);
		return true;
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
		if (!reply || reply->type != REDIS_REPLY_STRING)
		{
			LOG_ERROR << "Redis GET failed or no data for key: " << element->redis_key;
			loading_queue_.erase(element->redis_key);
			return;
		}

		element->message_value = CreateMessage();
		if (!element->message_value->ParseFromArray(reply->str, static_cast<int>(reply->len)))
		{
			LOG_ERROR << "ParseFromArray failed for key: " << element->redis_key;
		}

		loading_queue_.erase(element->redis_key);

		if (load_callback_with_extra_)
		{
			load_callback_with_extra_(element->message_key, *element->message_value, element->extra_data);
		}
		else if (load_callback_)
		{
			load_callback_(element->message_key, *element->message_value);
		}
	}


private:
	hiredis::Hiredis& hiredis_;
	LoadingQueue loading_queue_;
	EventCallback save_callback_;
	EventCallback load_callback_;
	EventCallbackWithExtra load_callback_with_extra_;
};

