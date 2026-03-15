#pragma once

#include <string>

namespace contracts::kafka {
class SceneCommand;
}

void DispatchSceneKafkaCommand(const std::string& topic, const contracts::kafka::SceneCommand& command);
