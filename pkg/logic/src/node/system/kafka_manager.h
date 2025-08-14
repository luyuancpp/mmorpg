// kafka_manager.h

#pragma once

#include <string>
#include <functional>
#include <memory>
#include "kafka/kafka_producer.h"
#include "kafka/kafka_consumer.h"

using KafkaHandler = std::function<void(const std::string& topic, const std::string& msg)>;

class KafkaConfig;

class KafkaManager {
public:
	bool Init(const KafkaConfig& config);
	bool Publish(const std::string& topic, const std::string& msg);
	void Shutdown();

	void SetKafkaHandler(const KafkaHandler& h) { kafkaHandler = h; }

private:
	KafkaHandler kafkaHandler;
};
