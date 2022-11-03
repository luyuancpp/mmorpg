#pragma once

#include "common.pb.h"

class LobbyConfig
{
public:
	const LobbyConfigInfo& config_info() const { return config_info_; }

	static LobbyConfig& GetSingleton() {
		thread_local LobbyConfig singleton;
		return singleton;
	}

	void Load(const std::string& filename);
private:
	LobbyConfigInfo config_info_;
};

