#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <rdkafkacpp.h>

namespace muduo { namespace net { class EventLoop; } }

class KafkaConsumer {
public:
	using MessageCallback = std::function<void(const std::string&, const std::string&)>;

	bool init(const std::string& brokers,
		const std::string& groupId,
		const std::vector<std::string>& topics,
		const std::vector<int32_t>& partitions,  // Partitions to consume
		const MessageCallback& callback);

	static KafkaConsumer& Instance() {
		thread_local KafkaConsumer instance;
		return instance;
	}

	~KafkaConsumer();

	bool start();
	void stop();

	// Non-blocking poll — keeps existing main-loop driver working while
	// callers migrate to background polling. Safe to call from the muduo
	// EventLoop. No-op if background polling is active.
	void poll();

	// Start a dedicated polling thread that drives consume() independently
	// of the muduo EventLoop. Decoded messages are dispatched via
	// EventLoop::queueInLoop on `dispatchLoop`, preserving the single-
	// threaded callback invariant the rest of the codebase relies on.
	//
	// Must be called AFTER init() succeeds. Idempotent: a second call is a
	// no-op once the thread is running.
	void startBackgroundPolling(muduo::net::EventLoop* dispatchLoop);

private:
	void backgroundPollLoop();
	void dispatch(const std::string& topic, std::string payload);
	void consumeOnce(bool blocking);

	std::unique_ptr<RdKafka::KafkaConsumer> consumer_;
	std::unique_ptr<RdKafka::Conf> conf_;
	MessageCallback msgCallback_;
	std::atomic<bool> running_{false};

	// Background poll thread state.
	std::thread pollThread_;
	std::atomic<bool> pollThreadRunning_{false};
	std::optional<std::reference_wrapper<muduo::net::EventLoop>> dispatchLoop_;
};
