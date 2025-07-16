#include "kafka_consumer.h"
#include "muduo/base/Logging.h"

KafkaConsumer::KafkaConsumer(const std::string& brokers, const std::string& groupId,
	const std::vector<std::string>& topics,
	const std::vector<int>& partitions,  // 需要消费的分区
	MessageCallback callback)
	: msgCallback_(std::move(callback)) {
	std::string errstr;

	// 配置 Kafka consumer
	conf_.reset(RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL));
	conf_->set("bootstrap.servers", brokers, errstr);
	conf_->set("group.id", groupId, errstr);
	conf_->set("enable.auto.commit", "true", errstr);
	conf_->set("auto.offset.reset", "earliest", errstr);  // 可根据需求改为 latest

	consumer_.reset(RdKafka::KafkaConsumer::create(conf_.get(), errstr));
	if (!consumer_) {
		LOG_ERROR << "Failed to create KafkaConsumer: " << errstr;
		return;
	}

	// 如果需要指定消费的分区，使用 assign()
	if (!partitions.empty()) {
		std::vector<RdKafka::TopicPartition*> assignedPartitions;
		for (int partition : partitions) {
			assignedPartitions.push_back(RdKafka::TopicPartition::create(topics[0], partition));
		}
		consumer_->assign(assignedPartitions);
		LOG_INFO << "Assigned to specific partitions.";
	}
	else {
		// 默认使用订阅方式，如果没有指定分区
		RdKafka::ErrorCode err = consumer_->subscribe(topics);
		if (err) {
			LOG_ERROR << "Failed to subscribe to topics: " << RdKafka::err2str(err);
		}
	}

	LOG_INFO << "KafkaConsumer initialized, ready to consume messages.";
}

KafkaConsumer::~KafkaConsumer() {
	stop();
}

void KafkaConsumer::start() {
	running_ = true;
}

void KafkaConsumer::stop() {
	if (running_) {
		running_ = false;
		if (consumer_) {
			consumer_->close();  // 正确关闭 consumer
		}
	}
}

void KafkaConsumer::poll() {
	if (!running_) return;

	// 非阻塞轮询，每次最多等待 100ms
	std::unique_ptr<RdKafka::Message> msg{ consumer_->consume(100) };
	if (!msg) return;

	switch (msg->err()) {
	case RdKafka::ERR_NO_ERROR:
		if (msgCallback_) {
			msgCallback_(msg->topic_name(), std::string(static_cast<const char*>(msg->payload()), msg->len()));
		}
		break;

	case RdKafka::ERR__TIMED_OUT:
		break;  // 正常空转等待

	default:
		LOG_ERROR << "[KafkaConsumer] Error: " << msg->errstr();
		break;
	}

}
