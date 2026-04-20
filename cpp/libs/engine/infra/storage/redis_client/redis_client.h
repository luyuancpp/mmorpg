#pragma once

#include <string>
#include <memory>
#include <vector>
#include <limits>

#include "muduo/base/Logging.h"
#include "muduo/contrib/hiredis/Hiredis.h"

#include "deps/hiredis/hiredis.h"

#include "engine/core/type_define/type_define.h"

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

	static constexpr int kMaxLoadRetries = 5;

	struct Element
	{
		MessageKey message_key;
		std::string redis_key;
		MessageValuePtr message_value;
		int retry_count = 0;
	};

	using ElementPtr = std::shared_ptr<Element>;
	using LoadingQueue = std::unordered_map<std::string, ElementPtr>;
	using EventCallback = std::function<void(MessageKey, MessageValue&)>;
	using FailedCallback = std::function<void(MessageKey)>;

	using HiredisPtr = std::unique_ptr<hiredis::Hiredis>;

	explicit MessageAsyncClient(HiredisPtr& hiredis)
		: hiredis_(hiredis)
	{
	}

	inline const std::string full_name() const { return std::string(MessageValue::GetDescriptor()->full_name()); }

	void SetSaveCallback(const EventCallback& cb) { save_callback_ = cb; }
	void SetLoadCallback(const EventCallback& cb) { load_callback_ = cb; }
	void SetLoadFailedCallback(const FailedCallback& cb) { load_failed_callback_ = cb; }


	void Save(const MessageValuePtr& message, const MessageKey& key)
	{
		if (!hiredis_ || !hiredis_->connected())
		{
			LOG_WARN << "Redis not connected, dropping Save for key " << key;
			return;
		}

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

		// Atomically SET and SADD via Lua script
		hiredis_->command(std::bind(&MessageAsyncClient::OnSaved, this, std::placeholders::_1, std::placeholders::_2, element),
			"EVAL %s 1 %b %b",
			kSaveAndMarkLuaScript,
			element->redis_key.c_str(), element->redis_key.length(),
			message_cached_array.data(), message_cached_array.size()
		);
	}

	void AsyncLoad(const MessageKey& key, int retry_count = 0)
	{
		std::string redis_key = full_name() + ":" + std::to_string(key);

		if (!hiredis_ || !hiredis_->connected())
		{
			LOG_WARN << "Redis not connected, queueing AsyncLoad for retry: " << redis_key;
			auto element = std::make_shared<Element>();
			element->message_key = key;
			element->redis_key = redis_key;
			element->retry_count = retry_count;
			pending_retry_queue_[redis_key] = element;
			return;
		}

		// Already in-flight or waiting for retry
		if (loading_queue_.find(redis_key) != loading_queue_.end())
		{
			return;
		}
		pending_retry_queue_.erase(redis_key);

		ElementPtr element = std::make_shared<Element>();
		element->message_key = key;
		element->redis_key = redis_key;
		element->retry_count = retry_count;

		loading_queue_.emplace(redis_key, element);

		const std::string format = "GET " + redis_key;
		hiredis_->command(std::bind(&MessageAsyncClient::OnLoaded, this, std::placeholders::_1, std::placeholders::_2, element),
			format.c_str());
	}

	// Call after Redis reconnect to retry all pending/stale loads.
	void RetryPendingLoads()
	{
		if (!hiredis_ || !hiredis_->connected())
		{
			return;
		}

		// Move stale loading_queue_ entries to retry queue
		// (their callbacks will never fire after a disconnect)
		for (auto &[k, v] : loading_queue_)
		{
			pending_retry_queue_[k] = v;
		}
		loading_queue_.clear();

		if (pending_retry_queue_.empty())
		{
			return;
		}

		LOG_INFO << "RetryPendingLoads: retrying " << pending_retry_queue_.size() << " loads";

		// Copy and clear before issuing commands (AsyncLoad modifies pending_retry_queue_)
		auto pending = std::move(pending_retry_queue_);
		pending_retry_queue_.clear();
		for (auto &[key, element] : pending)
		{
			AsyncLoad(element->message_key);
		}
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
		loading_queue_.erase(element->redis_key);

		if (!reply || reply->type == REDIS_REPLY_ERROR)
		{
			LOG_ERROR << "Redis GET error for key: " << element->redis_key
					  << (reply ? (std::string(" err=") + reply->str) : " (null reply)");
			if (load_failed_callback_)
			{
				load_failed_callback_(element->message_key);
			}
			return;
		}

		if (reply->type == REDIS_REPLY_NIL)
		{
			if (element->retry_count < kMaxLoadRetries)
			{
				LOG_WARN << "Redis GET returned NIL for key: " << element->redis_key
						 << ", retry " << (element->retry_count + 1) << "/" << kMaxLoadRetries;
				pending_retry_queue_[element->redis_key] = element;
				element->retry_count++;
				return;
			}

			LOG_ERROR << "Redis GET returned NIL for key: " << element->redis_key
					  << ", exhausted all " << kMaxLoadRetries << " retries";
			if (load_failed_callback_)
			{
				load_failed_callback_(element->message_key);
			}
			return;
		}

		if (reply->type == REDIS_REPLY_STRING)
		{
			element->message_value = CreateMessage();
			if (reply->len > static_cast<size_t>(std::numeric_limits<int>::max()))
			{
				LOG_ERROR << "Redis payload too large for ParseFromArray, key: " << element->redis_key
						  << ", len=" << reply->len;
			}
			else if (!element->message_value->ParseFromArray(reply->str, static_cast<int>(reply->len)))
			{
				LOG_ERROR << "ParseFromArray failed for key: " << element->redis_key;
			}
		}
		else
		{
			LOG_ERROR << "Redis GET unexpected reply type=" << reply->type << " for key: " << element->redis_key;
			if (load_failed_callback_)
			{
				load_failed_callback_(element->message_key);
			}
			return;
		}

		if (load_callback_)
		{
			load_callback_(element->message_key, *element->message_value);
		}
	}

private:
	HiredisPtr& hiredis_;
	LoadingQueue loading_queue_;
	LoadingQueue pending_retry_queue_;
	EventCallback save_callback_;
	EventCallback load_callback_;
	FailedCallback load_failed_callback_;
};

