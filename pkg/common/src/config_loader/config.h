#pragma once

#include <string>

#include "proto/common/config.pb.h"

// 读取基础部署配置
bool readBaseDeployConfig(const std::string& filename, BaseDeployConfig& baseConfig);

// 读取游戏配置
bool readGameConfig(const std::string& filename, GameConfig& gameConfig);
