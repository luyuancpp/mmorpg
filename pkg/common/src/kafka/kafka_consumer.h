#pragma once

#include <string>
#include <vector>
#include <atomic>
#include <functional>
#include <rdkafkacpp.h>
#include <memory>

class KafkaConsumer {
public:
	using MessageCallback = std::function<void(const std::string& topic, const std::string& message)>;

	KafkaConsumer(const std::string& brokers, const std::string& groupId,
		const std::vector<std::string>& topics,
		MessageCallback callback);

	~KafkaConsumer();

	void start();   // 启动消费
	void stop();    // 停止消费
	void poll();    // 非阻塞轮询，供外部调用

private:
	void consumeLoop();  // 消费循环（如果需要，可以在此处做具体逻辑）

	std::unique_ptr<RdKafka::KafkaConsumer> consumer_;
	std::unique_ptr<RdKafka::Conf> conf_;
	std::atomic<bool> running_{ false };
	MessageCallback msgCallback_;
};
