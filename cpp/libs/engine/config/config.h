#pragma once

#include <string>

#include "proto/common/base/config.pb.h"

bool readBaseDeployConfig(const std::string &filename, BaseDeployConfig &baseConfig);

bool readGameConfig(const std::string &filename, GameConfig &gameConfig);

std::string GetConfigDir();
std::string GetDataRootDir();