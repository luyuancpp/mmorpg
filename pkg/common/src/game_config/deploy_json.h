#pragma once

#include "proto/common/deploy_config.pb.h"

class ZoneConfig
{
public:
    inline const GameConfigInfo& config_info() const { return config_info_; }
    inline decltype(auto) server_type() const {  return config_info_.server_type();  }

    static ZoneConfig& GetSingleton() {
        static ZoneConfig singleton;
        return singleton;
    }

    void Load(const std::string& filename);
private:
    GameConfigInfo config_info_;
};

class DeployConfig
{
public:
    const ConnetionParam& deploy_info() { return connetion_param_; }

    static DeployConfig& GetSingleton() {
        static DeployConfig singleton;
        return singleton;
    }

    void Load(const std::string& filename);

private:
    ConnetionParam connetion_param_;
};


