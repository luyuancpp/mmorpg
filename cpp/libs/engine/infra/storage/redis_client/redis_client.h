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
		// Pre-serialized payload (Save path only). Populated by Save() so the
		// retry path doesn't have to re-serialize the protobuf message.
		std::vector<uint8_t> serialized_payload;
		int retry_count = 0;
		// Earliest time this element is allowed to be retried (steady_clock).
		// Set when the element is enqueued into pending_retry_queue_ /
		// pending_save_queue_.
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
		ElementPtr element = std::make_shared<Element>();
		element->message_key = key;
		element->redis_key = full_name() + ":" + std::to_string(key);
		element->message_value = message;

		// Serialize once; retry path re-uses this buffer.
		const size_t size = message->ByteSizeLong();
		element->serialized_payload.resize(size);
		if (size > 0 && !message->SerializeToArray(element->serialized_payload.data(), static_cast<int>(size)))
		{
			LOG_ERROR << "SerializeToArray failed for key " << key;
			return;
		}

		if (!hiredis_ || !hiredis_->connected())
		{
			LOG_WARN << "Redis not connected, queueing Save for retry: " << element->redis_key;
			// next_retry_at default-constructed (epoch); periodic timer will pick it up
			// as soon as Redis reconnects.
			pending_save_queue_[element->redis_key] = element;
			return;
		}

		IssueSave(element);
	}

	void AsyncLoad(const MessageKey& key, int retry_count = 0)
	{
		std::string redis_key = full_name() + ":" + std::to_string(key);

		LOG_DEBUG << "AsyncLoad: key=" << redis_key
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
			// next_retry_at default-constructed (epoch); periodic timer will pick it up
			// as soon as Redis reconnects.
			pending_retry_queue_[redis_key] = element;
			return;
		}

		// Already in-flight
		if (loading_queue_.find(redis_key) != loading_queue_.end())
		{
			return;
		}

		// If the same key is already waiting in pending_retry_queue_, preserve its
		// retry_count so an external re-trigger does not reset NIL backoff.
		if (auto pendingIt = pending_retry_queue_.find(redis_key); pendingIt != pending_retry_queue_.end())
		{
			if (pendingIt->second->retry_count > retry_count)
			{
				retry_count = pendingIt->second->retry_count;
			}
			pending_retry_queue_.erase(pendingIt);
		}

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
	// (whose callbacks will never fire post-disconnect) into the retry queue,
	// re-issues every pending load immediately, and re-flushes pending saves.
	void OnReconnected()
	{
		if (!hiredis_ || !hiredis_->connected())
		{
			return;
		}

		// Cached EVALSHA hash is bound to the previous server connection;
		// drop it so we re-SCRIPT LOAD on the new connection.
		script_sha1_.clear();
		EnsureScriptLoaded();

		const auto now = std::chrono::steady_clock::now();
		for (auto &[k, v] : loading_queue_)
		{
			v->next_retry_at = now;
			pending_retry_queue_[k] = v;
		}
		loading_queue_.clear();

		const size_t pendingLoads = pending_retry_queue_.size();
		const size_t pendingSaves = pending_save_queue_.size();
		if (pendingLoads + pendingSaves == 0)
		{
			return;
		}

		LOG_INFO << "OnReconnected: re-issuing " << pendingLoads << " loads, "
				 << pendingSaves << " saves";
		FlushDuePending(now, /*force=*/true);
	}

	// Periodic timer entry point. ONLY drains entries in pending_retry_queue_
	// and pending_save_queue_ whose backoff has elapsed. MUST NOT touch
	// loading_queue_, otherwise in-flight GETs get re-issued and load_callback_
	// fires twice for the same key.
	void RetryDuePending()
	{
		if (!hiredis_ || !hiredis_->connected())
		{
			return;
		}
		if (pending_retry_queue_.empty() && pending_save_queue_.empty())
		{
			return;
		}
		FlushDuePending(std::chrono::steady_clock::now(), /*force=*/false);
	}

	// Backwards-compatible alias.
	void RetryDuePendingLoads() { RetryDuePending(); }

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
		// Drain due loads
		std::vector<ElementPtr> dueLoads;
		dueLoads.reserve(pending_retry_queue_.size());
		for (auto it = pending_retry_queue_.begin(); it != pending_retry_queue_.end();)
		{
			if (force || it->second->next_retry_at <= now)
			{
				dueLoads.push_back(it->second);
				it = pending_retry_queue_.erase(it);
			}
			else
			{
				++it;
			}
		}

		// Drain due saves
		std::vector<ElementPtr> dueSaves;
		dueSaves.reserve(pending_save_queue_.size());
		for (auto it = pending_save_queue_.begin(); it != pending_save_queue_.end();)
		{
			if (force || it->second->next_retry_at <= now)
			{
				dueSaves.push_back(it->second);
				it = pending_save_queue_.erase(it);
			}
			else
			{
				++it;
			}
		}

		if (dueLoads.empty() && dueSaves.empty())
		{
			return;
		}
		LOG_INFO << "RetryDuePending: re-issuing " << dueLoads.size() << " loads, "
				 << dueSaves.size() << " saves "
				 << "(deferred loads=" << pending_retry_queue_.size()
				 << " saves=" << pending_save_queue_.size() << ")";
		for (auto &element : dueLoads)
		{
			AsyncLoad(element->message_key, element->retry_count);
		}
		for (auto &element : dueSaves)
		{
			IssueSave(element);
		}
	}

	// Lazily load the SET+SADD Lua script and cache its SHA1 for EVALSHA.
	// Called eagerly from OnReconnected and lazily from IssueSave when sha is empty.
	void EnsureScriptLoaded()
	{
		if (!script_sha1_.empty() || script_load_in_flight_)
		{
			return;
		}
		if (!hiredis_ || !hiredis_->connected())
		{
			return;
		}
		script_load_in_flight_ = true;
		hiredis_->command(std::bind(&MessageAsyncClient::OnSaveScriptLoaded, this, std::placeholders::_1, std::placeholders::_2),
						  "SCRIPT LOAD %s", kSaveAndMarkLuaScript);
	}

	void OnSaveScriptLoaded(hiredis::Hiredis * /*c*/, redisReply *reply)
	{
		script_load_in_flight_ = false;
		if (!reply || reply->type != REDIS_REPLY_STRING)
		{
			LOG_ERROR << "SCRIPT LOAD failed for " << full_name()
					  << " (reply " << (reply ? std::to_string(reply->type) : std::string("null")) << "); will retry on next save";
			return;
		}
		script_sha1_.assign(reply->str, reply->len);
		LOG_INFO << "SCRIPT LOAD ok for " << full_name() << " sha1=" << script_sha1_;
	}

	void IssueSave(const ElementPtr &element)
	{
		if (!hiredis_ || !hiredis_->connected())
		{
			pending_save_queue_[element->redis_key] = element;
			return;
		}

		EnsureScriptLoaded();

		int ret = REDIS_OK;
		if (!script_sha1_.empty())
		{
			ret = hiredis_->command(std::bind(&MessageAsyncClient::OnSaved, this, std::placeholders::_1, std::placeholders::_2, element),
									"EVALSHA %b 1 %b %b",
									script_sha1_.data(), script_sha1_.size(),
									element->redis_key.c_str(), element->redis_key.length(),
									element->serialized_payload.data(), element->serialized_payload.size());
		}
		else
		{
			// Fall back to EVAL while SCRIPT LOAD is in flight or after a failure.
			ret = hiredis_->command(std::bind(&MessageAsyncClient::OnSaved, this, std::placeholders::_1, std::placeholders::_2, element),
									"EVAL %s 1 %b %b",
									kSaveAndMarkLuaScript,
									element->redis_key.c_str(), element->redis_key.length(),
									element->serialized_payload.data(), element->serialized_payload.size());
		}

		if (ret != REDIS_OK)
		{
			LOG_ERROR << "Redis Save command failed (ret=" << ret << ") for key: " << element->redis_key;
			QueueSaveForRetry(element);
		}
	}

	static constexpr int kMaxSaveRetries = 6;

	void QueueSaveForRetry(const ElementPtr &element)
	{
		if (element->retry_count >= kMaxSaveRetries)
		{
			LOG_ERROR << "Save exhausted " << kMaxSaveRetries << " retries for key: " << element->redis_key
					  << " -- giving up; cache will be stale until dbservice rewrites";
			return;
		}
		const int nextRetry = element->retry_count + 1;
		const auto backoff = BackoffForRetry(nextRetry);
		element->retry_count = nextRetry;
		element->next_retry_at = std::chrono::steady_clock::now() + backoff;
		// Latest pending value per key wins (Save with same key replaces older retry).
		pending_save_queue_[element->redis_key] = element;
		LOG_WARN << "Queued Save retry " << nextRetry << "/" << kMaxSaveRetries
				 << " for key: " << element->redis_key << " in " << backoff.count() << "s";
	}

	void OnSaved(hiredis::Hiredis * /*c*/, redisReply *reply, ElementPtr element)
	{
		if (!reply)
		{
			LOG_ERROR << "Redis Save: null reply for key: " << element->redis_key;
			QueueSaveForRetry(element);
			return;
		}
		if (reply->type == REDIS_REPLY_ERROR)
		{
			const std::string err = reply->str ? reply->str : "";
			// NOSCRIPT: server flushed scripts (FLUSHALL/SCRIPT FLUSH/restart) -> reload and retry as EVAL once.
			if (err.compare(0, 8, "NOSCRIPT") == 0)
			{
				LOG_WARN << "EVALSHA NOSCRIPT for key: " << element->redis_key << " -- reloading script and retrying";
				script_sha1_.clear();
				EnsureScriptLoaded();
				// Immediate retry as EVAL (do not increment retry_count for this case).
				const int ret = hiredis_->command(std::bind(&MessageAsyncClient::OnSaved, this, std::placeholders::_1, std::placeholders::_2, element),
												  "EVAL %s 1 %b %b",
												  kSaveAndMarkLuaScript,
												  element->redis_key.c_str(), element->redis_key.length(),
												  element->serialized_payload.data(), element->serialized_payload.size());
				if (ret != REDIS_OK)
				{
					QueueSaveForRetry(element);
				}
				return;
			}
			LOG_ERROR << "Redis Save error for key: " << element->redis_key << " err=" << err;
			QueueSaveForRetry(element);
			return;
		}
		if (save_callback_)
		{
			save_callback_(element->message_key, *element->message_value);
		}
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
	LoadingQueue pending_save_queue_;
	std::string script_sha1_;
	bool script_load_in_flight_ = false;
	EventCallback save_callback_;
	EventCallback load_callback_;
	FailedCallback load_failed_callback_;
};

