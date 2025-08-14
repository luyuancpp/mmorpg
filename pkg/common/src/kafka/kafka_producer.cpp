#include "kafka_producer.h"
#include "muduo/base/Logging.h"

void KafkaProducer::init(const std::string& brokers) {
	std::string errstr;

	conf_.reset(RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL));
	if (conf_->set("bootstrap.servers", brokers, errstr) != RdKafka::Conf::CONF_OK) {
		LOG_ERROR << "Kafka conf error: " << errstr;
		return;
	}

	// 设置回调，非阻塞时用来检测消息是否送达
	conf_->set("dr_cb", this, errstr);

	producer_.reset(RdKafka::Producer::create(conf_.get(), errstr));
	if (!producer_) {
		LOG_ERROR << "Kafka producer create failed: " << errstr;
		return;
	}

	LOG_INFO << "KafkaProducer initialized for brokers: " << brokers;
}

KafkaProducer::~KafkaProducer() {
	// 等待所有消息发送完成再销毁（可选）
	while (producer_ && producer_->outq_len() > 0) {
		producer_->poll(100);
	}
}

RdKafka::ErrorCode KafkaProducer::send(const std::string& topic, const std::string& message,
	const std::string& key, int32_t partition) {
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

	poll(); // 保证事件分发

	return resp;
}


// Delivery callback 处理消息送达的状态
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

// 在主线程调用 poll()，处理消息队列中的所有消息
void KafkaProducer::poll() {
	// 轮询消息队列，0 表示非阻塞调用
	producer_->poll(0);
}
