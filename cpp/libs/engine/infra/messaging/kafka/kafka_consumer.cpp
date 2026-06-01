#include "kafka_consumer.h"
#include "muduo/base/Logging.h"
#include "muduo/net/EventLoop.h"

#include <chrono>
#include <string_view>
#include <unordered_map>

namespace
{
	constexpr int kMaxMessagesPerPoll = 128;
	// Suppress repeated "unknown topic" warnings during startup race between
	// consumer subscribe() and broker topic auto-creation. Log WARN at most once
	// per topic per kUnknownTopicWarnIntervalSec; otherwise log at DEBUG.
	constexpr int kUnknownTopicWarnIntervalSec = 30;
	// How long consume() blocks per call in background mode. Big enough that
	// idle traffic doesn't spin the CPU, small enough that stop() returns
	// promptly when the consumer is being shut down.
	constexpr int kBackgroundConsumeTimeoutMs = 200;
}

bool KafkaConsumer::init(const std::string &brokers, const std::string &groupId,
						 const std::vector<std::string> &topics,
						 const std::vector<int32_t> &partitions,
						 const MessageCallback &callback)
{
	msgCallback_ = callback;

	std::string errstr;

	// librdkafka rejects an empty group.id outright (returns CONF_INVALID with
	// "Invalid value for configuration property 'group.id'"). Callers were
	// hitting this during the eager Node::InitKafka() pass — the deploy yaml
	// keeps GroupID empty on purpose so per-node-id groups get derived later
	// (see node_kafka_command_handler.h:150). Detect it explicitly here so
	// the failure is one clear log line instead of a misleading rdkafka error.
	if (groupId.empty())
	{
		LOG_INFO << "KafkaConsumer::init skipped: empty group.id "
				 << "(per-node group will be set up later via "
				 << "RegisterKafkaCommandHandler).";
		return false;
	}

	conf_.reset(RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL));
	if (conf_->set("bootstrap.servers", brokers, errstr) != RdKafka::Conf::CONF_OK)
	{
		LOG_ERROR << "KafkaConsumer: failed to set bootstrap.servers: " << errstr;
		return false;
	}
	if (conf_->set("group.id", groupId, errstr) != RdKafka::Conf::CONF_OK)
	{
		LOG_ERROR << "KafkaConsumer: failed to set group.id: " << errstr;
		return false;
	}
	if (conf_->set("enable.auto.commit", "true", errstr) != RdKafka::Conf::CONF_OK)
	{
		LOG_ERROR << "KafkaConsumer: failed to set enable.auto.commit: " << errstr;
	}
	if (conf_->set("auto.offset.reset", "earliest", errstr) != RdKafka::Conf::CONF_OK)
	{
		LOG_ERROR << "KafkaConsumer: failed to set auto.offset.reset: " << errstr;
	}
	if (conf_->set("enable.sparse.connections", "true", errstr) != RdKafka::Conf::CONF_OK)
	{
		LOG_ERROR << "KafkaConsumer: failed to set enable.sparse.connections: " << errstr;
	}
	if (conf_->set("allow.auto.create.topics", "true", errstr) != RdKafka::Conf::CONF_OK)
	{
		LOG_ERROR << "KafkaConsumer: failed to set allow.auto.create.topics: " << errstr;
	}

	// Tune liveness for the muduo single-thread polling model. The consumer's
	// poll() runs on a 100ms timer on the same EventLoop that handles RPC,
	// client TCP, codec dispatch, and scene-response forwarding. Under 45k
	// concurrent connects (stress-1zone-45k-2026-05-30) the loop can stall
	// for many seconds, at which point the broker stops seeing the consumer:
	//
	//   * librdkafka maintains heartbeats from a background thread, so a
	//     short stall does NOT immediately drop the member. But once the
	//     loop misses poll() for longer than max.poll.interval.ms (default
	//     5 minutes), librdkafka voluntarily leaves the group and the
	//     coordinator marks "no active members" — that is exactly the
	//     symptom seen in the 45k run, with gate-group-1 lag spiking to
	//     17k while gate-2 was still accepting TCP.
	//   * Raising max.poll.interval.ms tolerates longer poll-skips, and
	//     setting session.timeout.ms + heartbeat.interval.ms explicitly
	//     locks in the heartbeat cadence so it doesn't drift across
	//     librdkafka versions.
	//   * cooperative-sticky keeps un-rebalanced partitions assigned during
	//     incremental rebalances; with the default eager strategy every
	//     rebalance revokes everything, which during a login storm
	//     compounds the lag by forcing a full re-assign each tick.
	struct LibrdkafkaTuning
	{
		std::string_view name;
		std::string_view value;
	};
	static constexpr LibrdkafkaTuning kTuning[] = {
		{"session.timeout.ms", "45000"},
		{"max.poll.interval.ms", "900000"},
		{"heartbeat.interval.ms", "3000"},
		{"partition.assignment.strategy", "cooperative-sticky"},
		// Coalesce broker fetches a bit so a tick that DOES get poll() time
		// drains more messages without raising the per-poll batch ceiling.
		{"fetch.min.bytes", "1"},
		{"fetch.wait.max.ms", "50"},
	};
	for (const auto &t : kTuning)
	{
		const std::string name(t.name);
		const std::string value(t.value);
		if (conf_->set(name, value, errstr) != RdKafka::Conf::CONF_OK)
		{
			LOG_WARN << "KafkaConsumer: failed to set " << name
					 << "=" << value << ": " << errstr;
		}
	}

	consumer_.reset(RdKafka::KafkaConsumer::create(conf_.get(), errstr));
	if (!consumer_)
	{
		LOG_ERROR << "Failed to create KafkaConsumer: " << errstr;
		return false;
	}

	// Use assign() for specific partitions
	if (!partitions.empty())
	{
		std::vector<RdKafka::TopicPartition *> assignedPartitions;
		for (int32_t partition : partitions)
		{
			if (topics.empty())
			{
				LOG_ERROR << "KafkaConsumer: topics is empty, cannot assign partitions";
				consumer_->close();
				consumer_.reset();
				return false;
			}
			assignedPartitions.push_back(RdKafka::TopicPartition::create(topics[0], partition));
		}
		const auto err = consumer_->assign(assignedPartitions);
		for (auto *assignedPartition : assignedPartitions)
		{
			delete assignedPartition;
		}
		if (err)
		{
			LOG_ERROR << "Failed to assign Kafka partitions: " << RdKafka::err2str(err);
			consumer_->close();
			consumer_.reset();
			return false;
		}
		LOG_INFO << "Assigned to specific partitions.";
	}
	else
	{
		// No specific partitions — use group-managed subscription
		RdKafka::ErrorCode err = consumer_->subscribe(topics);
		if (err)
		{
			LOG_ERROR << "Failed to subscribe to topics: " << RdKafka::err2str(err);
			consumer_->close();
			consumer_.reset();
			return false;
		}
	}

	LOG_INFO << "KafkaConsumer initialized, ready to consume messages.";
	return true;
}

KafkaConsumer::~KafkaConsumer()
{
	stop();
}

bool KafkaConsumer::start()
{
	if (!consumer_)
	{
		LOG_ERROR << "KafkaConsumer start requested before successful initialization.";
		running_ = false;
		return false;
	}

	running_ = true;
	return true;
}

void KafkaConsumer::stop()
{
	const bool wasRunning = running_.exchange(false);

	// Tear down background poll thread first. The thread checks running_
	// every loop iteration and bails out within kBackgroundConsumeTimeoutMs,
	// so this join is short even mid-fetch.
	if (pollThreadRunning_.exchange(false))
	{
		if (pollThread_.joinable())
		{
			pollThread_.join();
		}
	}

	if (wasRunning && consumer_)
	{
		consumer_->close();
	}
}

void KafkaConsumer::startBackgroundPolling(muduo::net::EventLoop* dispatchLoop)
{
	if (!consumer_ || !running_)
	{
		LOG_ERROR << "KafkaConsumer::startBackgroundPolling called before init/start.";
		return;
	}
	if (!dispatchLoop)
	{
		LOG_ERROR << "KafkaConsumer::startBackgroundPolling requires a non-null dispatch loop.";
		return;
	}
	if (pollThreadRunning_.exchange(true))
	{
		// Already running — refuse to double-spawn. The thread-local singleton
		// pattern guarantees there's only one consumer per worker thread, so a
		// re-init() + restart sequence calls stop() first which clears the flag.
		LOG_INFO << "KafkaConsumer background polling already active; ignoring duplicate start.";
		return;
	}

	dispatchLoop_ = std::ref(*dispatchLoop);
	pollThread_ = std::thread([this]() {
		backgroundPollLoop();
	});
	LOG_INFO << "KafkaConsumer background polling thread started.";
}

void KafkaConsumer::backgroundPollLoop()
{
	// Blocking consume on a dedicated thread. We hold no muduo locks here;
	// decoded payloads are handed back to the EventLoop via queueInLoop so the
	// existing callback (which touches ECS state) keeps running on the main
	// thread. This is the structural fix for the 45k stress finding: the old
	// 100ms EventLoop timer driver missed poll() under client-connect storms,
	// blowing past max.poll.interval.ms and getting the consumer kicked out
	// of the group.
	while (pollThreadRunning_.load(std::memory_order_acquire) && running_.load(std::memory_order_acquire))
	{
		consumeOnce(/*blocking=*/true);
	}
}

void KafkaConsumer::dispatch(const std::string& topic, std::string payload)
{
	if (!msgCallback_)
	{
		return;
	}
	if (dispatchLoop_)
	{
		// Move payload into the queued lambda so we don't double-copy the
		// rdkafka buffer. topic is small (broker topic name) so copy is fine.
		dispatchLoop_->get().queueInLoop(
			[cb = msgCallback_, topic, p = std::move(payload)]() mutable {
				cb(topic, p);
			});
	}
	else
	{
		// Foreground driver path (legacy poll()) — caller already on main loop.
		msgCallback_(topic, payload);
	}
}

void KafkaConsumer::consumeOnce(bool blocking)
{
	std::unique_ptr<RdKafka::Message> msg{
		consumer_->consume(blocking ? kBackgroundConsumeTimeoutMs : 0)};
	if (!msg)
	{
		return;
	}

	switch (msg->err())
	{
	case RdKafka::ERR_NO_ERROR:
		dispatch(msg->topic_name(),
			std::string(static_cast<const char *>(msg->payload()), msg->len()));
		break;

	case RdKafka::ERR__TIMED_OUT:
		return;

	case RdKafka::ERR__UNKNOWN_TOPIC:
	case RdKafka::ERR_UNKNOWN_TOPIC_OR_PART:
	{
		static thread_local std::unordered_map<std::string, std::chrono::steady_clock::time_point> sLastWarnByTopic;
		const std::string topic = msg->topic_name();
		const auto now = std::chrono::steady_clock::now();
		auto it = sLastWarnByTopic.find(topic);
		const bool shouldWarn = (it == sLastWarnByTopic.end()) ||
								(std::chrono::duration_cast<std::chrono::seconds>(now - it->second).count() >= kUnknownTopicWarnIntervalSec);
		if (shouldWarn)
		{
			sLastWarnByTopic[topic] = now;
			LOG_WARN << "[KafkaConsumer] Subscribed topic not available yet: " << msg->errstr();
		}
		else
		{
			LOG_DEBUG << "[KafkaConsumer] Subscribed topic not available yet: " << msg->errstr();
		}
		return;
	}

	default:
		LOG_ERROR << "[KafkaConsumer] Error: " << msg->errstr();
		break;
	}
}

void KafkaConsumer::poll()
{
	// Foreground driver. Once startBackgroundPolling() takes over, the timer
	// caller is harmless — we still drain whatever the background thread may
	// have left queued, but consume() is no-op-cheap when the rdkafka queue
	// is empty.
	if (!running_ || !consumer_)
		return;

	// If background polling owns the consumer, skip — the thread is already
	// draining records, and a second concurrent consume() across threads on
	// the same RdKafka::KafkaConsumer is unsafe.
	if (pollThreadRunning_.load(std::memory_order_acquire))
	{
		return;
	}

	for (int i = 0; i < kMaxMessagesPerPoll; ++i)
	{
		consumeOnce(/*blocking=*/false);
		// consumeOnce hides err/timeout state inside; the worst case is we
		// iterate kMaxMessagesPerPoll times with no work, which the rdkafka
		// queue check makes cheap.
	}
}
