#include "node_system.h"
#include <muduo/base/Logging.h>

eNodeType NodeSystem::GetServiceTypeFromPrefix(const std::string& prefix) {
	if (prefix.find("deploy") != std::string::npos) {
		return eNodeType::kDeploy;
	}
	else if (prefix.find("scene") != std::string::npos) {
		return  eNodeType::kSceneNode;
	}
	else if (prefix.find("gate") != std::string::npos) {
		return eNodeType::kGateNode;
	}
	else if (prefix.find("centre") != std::string::npos) {
		return eNodeType::kCentreNode;
	}
	else {
		LOG_ERROR << "Unknown service type for prefix: " << prefix;
	}

	return eNodeType::kCentreNode;
}