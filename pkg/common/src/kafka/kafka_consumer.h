#pragma once

#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <functional>
#include <rdkafkacpp.h>

class KafkaConsumer {
public:
	using MessageCallback = std::function<void(const std::string& topic, const std::string& message)>;

	KafkaConsumer(const std::string& brokers, const std::string& groupId,
		const std::vector<std::string>& topics,
		MessageCallback callback);

	~KafkaConsumer();

	void start();   // 启动消费线程
	void stop();    // 停止消费线程

private:
	void consumeLoop();

	std::unique_ptr<RdKafka::KafkaConsumer> consumer_;
	std::unique_ptr<RdKafka::Conf> conf_;
	std::unique_ptr<RdKafka::Conf> tconf_;

	std::thread consumerThread_;
	std::atomic<bool> running_{ false };
	MessageCallback msgCallback_;
};
