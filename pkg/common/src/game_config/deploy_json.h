#pragma once

#include "proto/common/deploy_config.pb.h"

class ZoneConfig
{
public:
    // 获取配置详情
    const GameConfigInfo& ConfigInfo() const { return configInfo; }

    // 获取服务器类型
    decltype(auto) ServerType() const { return configInfo.server_type(); }

    // 获取单例对象
    static ZoneConfig& GetSingleton() {
        static ZoneConfig singleton;
        return singleton;
    }

    // 加载配置文件
    void Load(const std::string& filename);

private:
    // 存储游戏配置
    GameConfigInfo configInfo;
};

class DeployConfig
{
public:
    // 获取部署配置信息
    const ConnetionParam& DeployInfo() const { return connectionParam; }

    // 获取单例对象
    static DeployConfig& GetSingleton() {
        static DeployConfig singleton;
        return singleton;
    }

    // 加载配置文件
    void Load(const std::string& filename);

private:
    // 存储连接参数
    ConnetionParam connectionParam;
};
