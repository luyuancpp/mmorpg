#pragma once

#include <chrono>
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

	// Max NIL retries. Combined with exponential backoff (2,4,8,16,32,60s) this
	// gives ~2 minutes for the DB write-back path (Kafka -> db service -> Redis)
	// to land the row before we permanently fail the load.
	static constexpr int kMaxLoadRetries = 6;

	struct Element
	{
		MessageKey message_key;
		std::string redis_key;
		MessageValuePtr message_value;
		int retry_count = 0;
		// Earliest time this element is allowed to be retried (steady_clock).
		// Set when the element is enqueued into pending_retry_queue_.
		std::chrono::steady_clock::time_point next_retry_at{};
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

		LOG_INFO << "AsyncLoad: key=" << redis_key
				 << " connected=" << (hiredis_ ? hiredis_->connected() : false)
				 << " retry=" << retry_count
				 << " loading_queue=" << loading_queue_.size()
				 << " pending_retry=" << pending_retry_queue_.size();

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
		int ret = hiredis_->command(std::bind(&MessageAsyncClient::OnLoaded, this, std::placeholders::_1, std::placeholders::_2, element),
			format.c_str());
		if (ret != REDIS_OK)
		{
			LOG_ERROR << "Redis command failed (ret=" << ret << ") for key: " << redis_key
					  << ", connected=" << hiredis_->connected();
			loading_queue_.erase(redis_key);
			pending_retry_queue_[redis_key] = element;
		}
	}

	// Call ONLY from the Redis reconnect callback. Migrates in-flight loads
	// (whose callbacks will never fire post-disconnect) into the retry queue
	// and re-issues every pending load immediately.
	void OnReconnected()
	{
		if (!hiredis_ || !hiredis_->connected())
		{
			return;
		}

		const auto now = std::chrono::steady_clock::now();
		for (auto &[k, v] : loading_queue_)
		{
			v->next_retry_at = now;
			pending_retry_queue_[k] = v;
		}
		loading_queue_.clear();

		if (pending_retry_queue_.empty())
		{
			return;
		}

		LOG_INFO << "OnReconnected: re-issuing " << pending_retry_queue_.size() << " pending loads";
		FlushDuePending(now, /*force=*/true);
	}

	// Periodic timer entry point. ONLY drains entries in pending_retry_queue_
	// whose backoff has elapsed. MUST NOT touch loading_queue_, otherwise
	// in-flight GETs get re-issued and load_callback_ fires twice for the
	// same key.
	void RetryDuePendingLoads()
	{
		if (!hiredis_ || !hiredis_->connected())
		{
			return;
		}
		if (pending_retry_queue_.empty())
		{
			return;
		}
		FlushDuePending(std::chrono::steady_clock::now(), /*force=*/false);
	}

	MessageValuePtr CreateMessage() { return std::make_shared<MessageValue>(); }

private:
	static std::chrono::seconds BackoffForRetry(int next_retry_count)
	{
		// 1->2s, 2->4s, 3->8s, 4->16s, 5->32s, 6+->60s
		switch (next_retry_count)
		{
		case 1:
			return std::chrono::seconds(2);
		case 2:
			return std::chrono::seconds(4);
		case 3:
			return std::chrono::seconds(8);
		case 4:
			return std::chrono::seconds(16);
		case 5:
			return std::chrono::seconds(32);
		default:
			return std::chrono::seconds(60);
		}
	}

	void FlushDuePending(std::chrono::steady_clock::time_point now, bool force)
	{
		std::vector<ElementPtr> due;
		due.reserve(pending_retry_queue_.size());
		for (auto it = pending_retry_queue_.begin(); it != pending_retry_queue_.end();)
		{
			if (force || it->second->next_retry_at <= now)
			{
				due.push_back(it->second);
				it = pending_retry_queue_.erase(it);
			}
			else
			{
				++it;
			}
		}
		if (due.empty())
		{
			return;
		}
		LOG_INFO << "RetryDuePendingLoads: re-issuing " << due.size()
				 << " loads (deferred=" << pending_retry_queue_.size() << ")";
		for (auto &element : due)
		{
			AsyncLoad(element->message_key, element->retry_count);
		}
	}

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
				const int nextRetry = element->retry_count + 1;
				const auto backoff = BackoffForRetry(nextRetry);
				LOG_WARN << "Redis GET returned NIL for key: " << element->redis_key
						 << ", retry " << nextRetry << "/" << kMaxLoadRetries
						 << " in " << backoff.count() << "s";
				element->retry_count = nextRetry;
				element->next_retry_at = std::chrono::steady_clock::now() + backoff;
				pending_retry_queue_[element->redis_key] = element;
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

