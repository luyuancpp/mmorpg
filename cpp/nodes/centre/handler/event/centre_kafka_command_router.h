#pragma once

#include <string>

namespace contracts::kafka {
class CentreCommand;
}

void DispatchCentreKafkaCommand(const std::string& topic, const contracts::kafka::CentreCommand& command);
