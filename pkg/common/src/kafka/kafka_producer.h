
#pragma once

#include <string>
#include <memory>
#include <rdkafkacpp.h>

class KafkaProducer : public RdKafka::DeliveryReportCb {
public:
	KafkaProducer(const std::string& brokers);
	~KafkaProducer();

	void send(const std::string& topic, const std::string& message);

	// Delivery callback（异步回调）
	void dr_cb(RdKafka::Message& message) override;

private:
	std::unique_ptr<RdKafka::Producer> producer_;
	std::unique_ptr<RdKafka::Conf> conf_;
};
