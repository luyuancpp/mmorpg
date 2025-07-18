#pragma once
#include <string>

class KafkaSystem {
public:
	static void KafkaMessageHandler(const std::string& topic, const std::string& message);
};