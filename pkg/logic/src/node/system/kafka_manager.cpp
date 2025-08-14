#include "kafka_manager.h"
#include "proto/common/config.pb.h"
#include <muduo/base/Logging.h>
#include <boost/algorithm/string/join.hpp>
#include "thread_local/storage_common_logic.h"
#include "thread_local/storage.h"

bool KafkaManager::Init(const KafkaConfig& config) {
	std::vector<std::string> brokersVec;
	for (const auto& broker : config.brokers()) {
		brokersVec.push_back(broker);
	}

	std::string brokers = boost::algorithm::join(brokersVec, ",");
	std::string groupId = config.group_id();

	std::vector<std::string> topicsVec;
	for (const auto& topic : config.topics()) {
		topicsVec.push_back(topic);
	}

	auto zoneId = tlsCommonLogic.GetGameConfig().zone_id(); // zone ID
	std::vector<int32_t> partitions{ static_cast<int32_t>(zoneId) };

	tls.GetKafkaProducer() = std::make_unique<KafkaProducer>(brokers);

	if (!kafkaHandler) {
		LOG_ERROR << "KafkaManager: Message handler not set. Please call subscribe() to register a handler.";
		return false;
	}

	tls.GetKafkaConsumer() = std::make_unique<KafkaConsumer>(
		brokers,
		groupId,
		topicsVec,
		partitions,
		kafkaHandler
	);

	tls.GetKafkaConsumer()->start();

	LOG_INFO << "KafkaManager initialized successfully. Brokers: " << brokers;
	return true;
}

bool KafkaManager::Publish(const std::string& topic, const std::string& msg) {
	if (!tls.GetKafkaProducer()) {
		LOG_ERROR << "KafkaManager: Producer is not initialized.";
		return false;
	}

	auto err = tls.GetKafkaProducer()->send(topic, msg);
	if (err != RdKafka::ERR_NO_ERROR) {
		LOG_ERROR << "KafkaManager: Failed to send message. Error: " << RdKafka::err2str(err);
		return false;
	}

	return true;
}

void KafkaManager::Shutdown() {
	if (tls.GetKafkaConsumer()) {
		tls.GetKafkaConsumer()->stop();
		tls.GetKafkaConsumer().reset();
	}

	if (tls.GetKafkaProducer()) {
		tls.GetKafkaProducer()->poll(); // flush if needed
		tls.GetKafkaProducer().reset();
	}

	LOG_INFO << "KafkaManager has been shut down.";
}
