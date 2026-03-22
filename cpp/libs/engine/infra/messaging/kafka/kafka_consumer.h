#pragma once

#include <string>
#include <memory>
#include <functional>
#include <vector>
#include <rdkafkacpp.h>

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

	// Non-blocking poll
	void poll();

private:
	std::unique_ptr<RdKafka::KafkaConsumer> consumer_;
	std::unique_ptr<RdKafka::Conf> conf_;
	MessageCallback msgCallback_;
	bool running_ = false;
};
