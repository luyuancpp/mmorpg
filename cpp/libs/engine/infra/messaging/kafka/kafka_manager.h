// kafka_manager.h

#pragma once

#include <string>
#include <functional>

using KafkaMessageCallback = std::function<void(const std::string& topic, const std::string& payload)>;

class KafkaConfig;

class KafkaManager {
public:
	bool Init(const KafkaConfig& config);
	bool Subscribe(const KafkaConfig& config,
		const std::vector<std::string>& topics,
		const std::string& groupId = {},
		const std::vector<int32_t>& partitions = {},
		KafkaMessageCallback callback = {});
	bool Publish(const std::string& topic, const std::string& msg);
	void Poll();
	void Shutdown();
};
