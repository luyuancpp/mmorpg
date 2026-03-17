#include "kafka_consumer.h"
#include "muduo/base/Logging.h"

namespace {
constexpr int kMaxMessagesPerPoll = 128;
}

bool KafkaConsumer::init(const std::string& brokers, const std::string& groupId,
	const std::vector<std::string>& topics,
	const std::vector<int32_t>& partitions,  // 需要消费的分区
	const MessageCallback& callback) {
	msgCallback_ = callback;

	std::string errstr;

	// 配置 Kafka consumer
	conf_.reset(RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL));
	conf_->set("bootstrap.servers", brokers, errstr);
	conf_->set("group.id", groupId, errstr);
	conf_->set("enable.auto.commit", "true", errstr);
	conf_->set("auto.offset.reset", "earliest", errstr);
	conf_->set("enable.sparse.connections", "true", errstr);

	consumer_.reset(RdKafka::KafkaConsumer::create(conf_.get(), errstr));
	if (!consumer_) {
		LOG_ERROR << "Failed to create KafkaConsumer: " << errstr;
		return false;
	}

	// 如果需要指定消费的分区，使用 assign()
	if (!partitions.empty()) {
		std::vector<RdKafka::TopicPartition*> assignedPartitions;
		for (int32_t partition : partitions) {
			if (topics.empty()) {
				LOG_ERROR << "KafkaConsumer: topics is empty, cannot assign partitions";
				consumer_->close();
				consumer_.reset();
				return false;
			}
			assignedPartitions.push_back(RdKafka::TopicPartition::create(topics[0], partition));
		}
		const auto err = consumer_->assign(assignedPartitions);
		for (auto* assignedPartition : assignedPartitions) {
			delete assignedPartition;
		}
		if (err) {
			LOG_ERROR << "Failed to assign Kafka partitions: " << RdKafka::err2str(err);
			consumer_->close();
			consumer_.reset();
			return false;
		}
		LOG_INFO << "Assigned to specific partitions.";
	}
	else {
		// 默认使用订阅方式，如果没有指定分区
		RdKafka::ErrorCode err = consumer_->subscribe(topics);
		if (err) {
			LOG_ERROR << "Failed to subscribe to topics: " << RdKafka::err2str(err);
			consumer_->close();
			consumer_.reset();
			return false;
		}
	}

	LOG_INFO << "KafkaConsumer initialized, ready to consume messages.";
	return true;
}

KafkaConsumer::~KafkaConsumer() {
	stop();
}

bool KafkaConsumer::start() {
	if (!consumer_) {
		LOG_ERROR << "KafkaConsumer start requested before successful initialization.";
		running_ = false;
		return false;
	}

	running_ = true;
	return true;
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
	if (!running_ || !consumer_) return;

	for (int i = 0; i < kMaxMessagesPerPoll; ++i) {
		std::unique_ptr<RdKafka::Message> msg{ consumer_->consume(0) };
		if (!msg) return;

		switch (msg->err()) {
		case RdKafka::ERR_NO_ERROR:
			if (msgCallback_) {
				msgCallback_(msg->topic_name(), std::string(static_cast<const char*>(msg->payload()), msg->len()));
			}
			break;

		case RdKafka::ERR__TIMED_OUT:
			return;

		default:
			LOG_ERROR << "[KafkaConsumer] Error: " << msg->errstr();
			break;
		}
	}
}
