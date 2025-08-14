#pragma once

#include <string>
#include <memory>
#include <functional>
#include <rdkafkacpp.h>

class KafkaProducer : public RdKafka::DeliveryReportCb {
public:
	using DeliveryCallback = std::function<void(const std::string& topic, int32_t partition, int64_t offset, const std::string& message)>;
	~KafkaProducer();

	void init(const std::string& brokers);

	// ✅ 线程单例访问器
	static KafkaProducer& Instance() {
		// 每个线程维护一个 KafkaProducer 实例（第一次用时创建）
		thread_local KafkaProducer instance;
		return instance;
	}

	// 异步发送消息，支持指定分区和消息的 key
	RdKafka::ErrorCode send(const std::string& topic, const std::string& message, const std::string& key = "", int32_t partition = RdKafka::Topic::PARTITION_UA);

	// 轮询处理消息队列
	void poll();

	// Delivery callback（异步回调）
	void dr_cb(RdKafka::Message& message) override;

private:
	std::unique_ptr<RdKafka::Producer> producer_;
	std::unique_ptr<RdKafka::Conf> conf_;
};
