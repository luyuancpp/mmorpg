#include "kafka_producer.h"
#include "muduo/base/Logging.h"

void KafkaProducer::setBrokers(const std::string& brokers) {
	if (producer_) return; // already created
	pendingBrokers_ = brokers;
	LOG_INFO << "KafkaProducer: brokers stored for lazy init: " << brokers;
}

bool KafkaProducer::init(const std::string& brokers) {
	if (producer_) return true; // idempotent

	std::string errstr;

	conf_.reset(RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL));
	if (conf_->set("bootstrap.servers", brokers, errstr) != RdKafka::Conf::CONF_OK) {
		LOG_ERROR << "Kafka conf error: " << errstr;
		producer_.reset();
		return false;
	}

	if (conf_->set("dr_cb", this, errstr) != RdKafka::Conf::CONF_OK) {
		LOG_ERROR << "Kafka producer: failed to set dr_cb: " << errstr;
	}
	if (conf_->set("enable.sparse.connections", "true", errstr) != RdKafka::Conf::CONF_OK) {
		LOG_ERROR << "Kafka producer: failed to set enable.sparse.connections: " << errstr;
	}

	producer_.reset(RdKafka::Producer::create(conf_.get(), errstr));
	if (!producer_) {
		LOG_ERROR << "Kafka producer create failed: " << errstr;
		return false;
	}

	pendingBrokers_.clear();
	LOG_INFO << "KafkaProducer initialized for brokers: " << brokers;
	return true;
}

bool KafkaProducer::ensureInitialized() {
	if (producer_) return true;
	if (pendingBrokers_.empty()) return false;
	return init(pendingBrokers_);
}

KafkaProducer::~KafkaProducer() {
	// Flush pending messages before destruction
	while (producer_ && producer_->outq_len() > 0) {
		producer_->poll(100);
	}
}

RdKafka::ErrorCode KafkaProducer::send(const std::string& topic, const std::string& message,
	const std::string& key, int32_t partition) {
	if (!ensureInitialized()) {
		LOG_ERROR << "KafkaProducer send requested but no brokers configured.";
		return RdKafka::ERR__STATE;
	}

	RdKafka::ErrorCode resp = producer_->produce(
		topic,
		partition,
		RdKafka::Producer::RK_MSG_COPY,
		const_cast<char*>(message.c_str()),
		message.size(),
		key.empty() ? nullptr : const_cast<char*>(key.c_str()),
		key.size(),
		0,
		nullptr, nullptr);

	if (resp != RdKafka::ERR_NO_ERROR) {
		LOG_ERROR << "Produce failed: " << RdKafka::err2str(resp);
	}
	else {
		LOG_INFO << "[Kafka] Message queued to topic " << topic;
	}

	poll(); // Ensure delivery callbacks are dispatched

	return resp;
}


void KafkaProducer::dr_cb(RdKafka::Message& message) {
	if (message.err()) {
		LOG_ERROR << "[Kafka] Delivery failed: " << message.errstr();
	}
	else {
		LOG_INFO << "[Kafka] Delivered message to topic " << message.topic_name()
			<< " [" << message.partition() << "] at offset "
			<< message.offset();
	}
}

void KafkaProducer::poll() {
	if (!producer_) {
		return;
	}

	producer_->poll(0); // 0 = non-blocking
}
