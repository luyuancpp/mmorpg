#include "kafka_manager.h"
#include "proto/common/base/config.pb.h"
#include <muduo/base/Logging.h>
#include <boost/algorithm/string/join.hpp>
#include "thread_context/redis_manager.h"

#include <node_config_manager.h>

namespace {
std::string JoinBrokers(const KafkaConfig& config) {
	std::vector<std::string> brokersVec;
	for (const auto& broker : config.brokers()) {
		brokersVec.push_back(broker);
	}

	return boost::algorithm::join(brokersVec, ",");
}

std::vector<std::string> CollectTopics(const KafkaConfig& config) {
	std::vector<std::string> topicsVec;
	for (const auto& topic : config.topics()) {
		topicsVec.push_back(topic);
	}

	return topicsVec;
}
}

bool KafkaManager::Init(const KafkaConfig& config) {
	auto zoneId = tlsNodeConfigManager.GetGameConfig().zone_id();
	return Subscribe(
		config,
		CollectTopics(config),
		config.group_id(),
		{ static_cast<int32_t>(zoneId) }
	);
}

bool KafkaManager::Subscribe(const KafkaConfig& config,
	const std::vector<std::string>& topics,
	const std::string& groupId,
	const std::vector<int32_t>& partitions,
	KafkaMessageCallback callback) {
	if (!callback) {
		LOG_DEBUG << "KafkaManager: No message callback provided, skipping subscribe.";
		return false;
	}

	if (topics.empty()) {
		LOG_ERROR << "KafkaManager: Cannot subscribe without any topics configured.";
		return false;
	}

	const std::string brokers = JoinBrokers(config);
	const std::string effectiveGroupId = groupId.empty() ? config.group_id() : groupId;

	// Defer producer creation until first Publish() call to save rdkafka threads.
	KafkaProducer::Instance().setBrokers(brokers);

	KafkaConsumer::Instance().stop();
	if (!KafkaConsumer::Instance().init(
		brokers,
		effectiveGroupId,
		topics,
		partitions,
		callback
	)) {
		LOG_ERROR << "KafkaManager: Failed to initialize Kafka consumer for topics: "
			<< boost::algorithm::join(topics, ",");
		return false;
	}

	if (!KafkaConsumer::Instance().start()) {
		LOG_ERROR << "KafkaManager: Failed to start Kafka consumer for topics: "
			<< boost::algorithm::join(topics, ",");
		return false;
	}

	LOG_INFO << "KafkaManager initialized successfully. Brokers: " << brokers
		<< " Topics: " << boost::algorithm::join(topics, ",");
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

void KafkaManager::Poll() {
	KafkaConsumer::Instance().poll();
}

void KafkaManager::Shutdown() {
	KafkaConsumer::Instance().stop();

	if (KafkaProducer::Instance().initialized()) {
		KafkaProducer::Instance().poll(); // flush if needed
	}

	LOG_INFO << "KafkaManager has been shut down.";
}

