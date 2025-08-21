#pragma once

#include "config_loader/config.h"
#include "proto/common/message.pb.h"

class NodeConfigManager {

public:
	NodeConfigManager() = default;
	NodeConfigManager(const NodeConfigManager&) = delete;
	NodeConfigManager& operator=(const NodeConfigManager&) = delete;

	static NodeConfigManager& Instance() {
		thread_local NodeConfigManager instance;
		return instance;
	}

	[[nodiscard]] ::BaseDeployConfig& GetBaseDeployConfig()
	{
		return BaseDeployConfig;
	}

	[[nodiscard]] GameConfig& GetGameConfig()
	{
		return GameConfig;
	}

private:
	BaseDeployConfig BaseDeployConfig;
	GameConfig GameConfig;
};