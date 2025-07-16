// KafkaProducer.cpp

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

	LOG_INFO << "KafkaProducer initialized for brokers: " << brokers ;
}

KafkaProducer::~KafkaProducer() {
	// 等待所有消息发送完成再销毁（可选）
	while (producer_ && producer_->outq_len() > 0) {
		producer_->poll(100);
	}
}

void KafkaProducer::send(const std::string& topic, const std::string& message) {
	RdKafka::ErrorCode resp = producer_->produce(
		topic,
		RdKafka::Topic::PARTITION_UA,
		RdKafka::Producer::RK_MSG_COPY,  // 自动拷贝 payload
		const_cast<char*>(message.c_str()),
		message.size(),
		nullptr, 0, 0, nullptr, nullptr);

	if (resp != RdKafka::ERR_NO_ERROR) {
		LOG_ERROR << "Produce failed: " << RdKafka::err2str(resp) ;
		return;
	}
	else {
		// 非阻塞，成功排队即可返回
		LOG_INFO << "[Kafka] Message queued" ;
	}

	// 非阻塞，只调用一次 poll()（回调需要）
	producer_->poll(0);
}

void KafkaProducer::dr_cb(RdKafka::Message& message) {
	if (message.err()) {
		LOG_ERROR << "[Kafka] Delivery failed: " << message.errstr() ;
	}
	else {
		LOG_INFO << "[Kafka] Delivered message to topic " << message.topic_name()
			<< " [" << message.partition() << "] at offset "
			<< message.offset() ;
	}
}
