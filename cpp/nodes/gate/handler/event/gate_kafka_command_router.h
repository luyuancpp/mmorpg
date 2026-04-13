#pragma once

#include <string>

namespace contracts::kafka {
class GateCommand;
}

void DispatchGateKafkaCommand(const std::string& topic, const contracts::kafka::GateCommand& command);
