#include "kafka_producer.h"
#include "muduo/base/Logging.h"

KafkaProducer::KafkaProducer(const std::string& brokers) {
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

void KafkaProducer::send(const std::string& topic, const std::string& message, const std::string& key, int partition) {
	RdKafka::ErrorCode resp = producer_->produce(
		topic,              // 目标 topic
		partition,          // 指定分区
		RdKafka::Producer::RK_MSG_COPY,  // 自动拷贝 payload
		const_cast<char*>(message.c_str()),  // 消息内容
		message.size(),     // 消息大小
		key.empty() ? nullptr : const_cast<char*>(key.c_str()),  // 消息的 key
		key.size(),         // key 的大小
		0,                  // 时间戳 (如果有的话)
		nullptr, nullptr);  // 不使用消息头（如果有需要可以扩展）

	if (resp != RdKafka::ERR_NO_ERROR) {
		LOG_ERROR << "Produce failed: " << RdKafka::err2str(resp);
		return;
	}
	else {
		LOG_INFO << "[Kafka] Message queued to topic " << topic;
	}

	// 非阻塞，调用一次 poll()（回调需要）
	poll();
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
