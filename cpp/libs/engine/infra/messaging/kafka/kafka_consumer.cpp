#include "kafka_consumer.h"
#include "muduo/base/Logging.h"

namespace {
constexpr int kMaxMessagesPerPoll = 128;
}

bool KafkaConsumer::init(const std::string& brokers, const std::string& groupId,
	const std::vector<std::string>& topics,
	const std::vector<int32_t>& partitions,
	const MessageCallback& callback) {
	msgCallback_ = callback;

	std::string errstr;

	conf_.reset(RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL));
	if (conf_->set("bootstrap.servers", brokers, errstr) != RdKafka::Conf::CONF_OK) {
		LOG_ERROR << "KafkaConsumer: failed to set bootstrap.servers: " << errstr;
		return false;
	}
	if (conf_->set("group.id", groupId, errstr) != RdKafka::Conf::CONF_OK) {
		LOG_ERROR << "KafkaConsumer: failed to set group.id: " << errstr;
		return false;
	}
	if (conf_->set("enable.auto.commit", "true", errstr) != RdKafka::Conf::CONF_OK) {
		LOG_ERROR << "KafkaConsumer: failed to set enable.auto.commit: " << errstr;
	}
	if (conf_->set("auto.offset.reset", "earliest", errstr) != RdKafka::Conf::CONF_OK) {
		LOG_ERROR << "KafkaConsumer: failed to set auto.offset.reset: " << errstr;
	}
	if (conf_->set("enable.sparse.connections", "true", errstr) != RdKafka::Conf::CONF_OK) {
		LOG_ERROR << "KafkaConsumer: failed to set enable.sparse.connections: " << errstr;
	}

	consumer_.reset(RdKafka::KafkaConsumer::create(conf_.get(), errstr));
	if (!consumer_) {
		LOG_ERROR << "Failed to create KafkaConsumer: " << errstr;
		return false;
	}

	// Use assign() for specific partitions
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
		// No specific partitions — use group-managed subscription
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
			consumer_->close();
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
