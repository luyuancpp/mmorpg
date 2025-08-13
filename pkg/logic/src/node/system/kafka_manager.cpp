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

	auto zoneId = tlsCommonLogic.GetGameConfig().zone_id(); // 区服 ID
	std::vector<int32_t> partitions{ static_cast<int32_t>(zoneId)}; // KafkaConsumer 用 int32_t 分区

	tls.GetKafkaProducer() = std::make_unique<KafkaProducer>(brokers);

	if (!kafkaHandler) {
		LOG_ERROR << "KafkaManager: 消息处理器未设置。请先调用 subscribe() 注册 handler。";
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

	LOG_INFO << "KafkaManager 初始化完成，brokers: " << brokers;
	return true;
}

bool KafkaManager::Publish(const std::string& topic, const std::string& msg) {
	if (!tls.GetKafkaProducer()) {
		LOG_ERROR << "KafkaManager: 生产者未初始化";
		return false;
	}

	auto err = tls.GetKafkaProducer()->send(topic, msg);
	if (err != RdKafka::ERR_NO_ERROR) {
		LOG_ERROR << "KafkaManager: 发送失败, err=" << RdKafka::err2str(err);
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
		tls.GetKafkaProducer()->poll(); // flush 可能加在你自己的实现里
		tls.GetKafkaProducer().reset();
	}

	LOG_INFO << "KafkaManager 已关闭";
}
