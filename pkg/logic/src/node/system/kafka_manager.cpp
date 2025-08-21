#include "kafka_manager.h"
#include "proto/common/config.pb.h"
#include <muduo/base/Logging.h>
#include <boost/algorithm/string/join.hpp>
#include "thread_local/redis_manager.h"
#include "thread_local/storage.h"
#include <thread_local/node_config_manager.h>

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

	auto zoneId = NodeConfigManager::Instance().GetGameConfig().zone_id(); // zone ID
	std::vector<int32_t> partitions{ static_cast<int32_t>(zoneId) };

	KafkaProducer::Instance().init(brokers);

	if (!kafkaHandler) {
		LOG_DEBUG << "KafkaManager: Message handler not set. Please call subscribe() to register a handler.";
		return false;
	}

	KafkaConsumer::Instance().init(
		brokers,
		groupId,
		topicsVec,
		partitions,
		kafkaHandler
	);

	KafkaConsumer::Instance().start();

	LOG_INFO << "KafkaManager initialized successfully. Brokers: " << brokers;
	return true;
}

bool KafkaManager::Publish(const std::string& topic, const std::string& msg) {

	auto err = KafkaProducer::Instance().send(topic, msg);
	if (err != RdKafka::ERR_NO_ERROR) {
		LOG_ERROR << "KafkaManager: Failed to send message. Error: " << RdKafka::err2str(err);
		return false;
	}

	return true;
}

void KafkaManager::Shutdown() {
	KafkaConsumer::Instance().stop();

	KafkaProducer::Instance().poll(); // flush if needed
	
	LOG_INFO << "KafkaManager has been shut down.";
}
