#pragma once

#include <string>
#include <memory>
#include <functional>
#include <rdkafkacpp.h>

class KafkaProducer : public RdKafka::DeliveryReportCb {
public:
	using DeliveryCallback = std::function<void(const std::string& topic, int32_t partition, int64_t offset, const std::string& message)>;

	KafkaProducer(const std::string& brokers);
	~KafkaProducer();

	// 异步发送消息，支持指定分区和消息的 key
	void send(const std::string& topic, const std::string& message, const std::string& key = "", int32_t partition = RdKafka::Topic::PARTITION_UA);

	// 轮询处理消息队列
	void poll();

	// Delivery callback（异步回调）
	void dr_cb(RdKafka::Message& message) override;

private:
	std::unique_ptr<RdKafka::Producer> producer_;
	std::unique_ptr<RdKafka::Conf> conf_;
};
