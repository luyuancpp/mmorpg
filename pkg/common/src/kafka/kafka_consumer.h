#pragma once

#include <string>
#include <memory>
#include <functional>
#include <vector>
#include <rdkafkacpp.h>

class KafkaConsumer {
public:
	using MessageCallback = std::function<void(const std::string&, const std::string&)>;

	KafkaConsumer(const std::string& brokers, const std::string& groupId,
		const std::vector<std::string>& topics,
		const std::vector<int>& partitions,  // 指定需要消费的分区
		MessageCallback callback);

	~KafkaConsumer();

	void start();
	void stop();

	// 非阻塞轮询方法
	void poll();

private:
	std::unique_ptr<RdKafka::KafkaConsumer> consumer_;
	std::unique_ptr<RdKafka::Conf> conf_;
	MessageCallback msgCallback_;
	bool running_ = false;
};
