// kafka_manager.h

#pragma once

#include <string>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace muduo { namespace net { class EventLoop; } }

using KafkaMessageCallback = std::function<void(const std::string& topic, const std::string& payload)>;

class KafkaConfig;
class KafkaConsumer;

class KafkaManager {
public:
	KafkaManager();
	~KafkaManager();

	KafkaManager(const KafkaManager&) = delete;
	KafkaManager& operator=(const KafkaManager&) = delete;

	bool Init(const KafkaConfig& config);
	bool Subscribe(const KafkaConfig& config,
		const std::vector<std::string>& topics,
		const std::string& groupId = {},
		const std::vector<int32_t>& partitions = {},
		KafkaMessageCallback callback = {});
	bool Publish(const std::string& topic, const std::string& msg);

	// Cooperative poll from the muduo EventLoop. Becomes a near-no-op once
	// StartBackgroundPolling has been called.
	void Poll();

	// Move record consumption off the EventLoop onto a dedicated thread. The
	// `dispatchLoop` is where decoded message callbacks are queued so they
	// keep running on the same thread as the rest of the node state. Must be
	// called after Subscribe() succeeds; safe to call multiple times.
	void StartBackgroundPolling(muduo::net::EventLoop* dispatchLoop);

	void Shutdown();

private:
	std::vector<std::unique_ptr<KafkaConsumer>> consumers_;
	std::optional<std::reference_wrapper<muduo::net::EventLoop>> dispatchLoop_;
};
