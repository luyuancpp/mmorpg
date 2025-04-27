#include "config.h"

#include <yaml-cpp/yaml.h>

// 读取基础部署配置
// 读取基础部署配置
bool readBaseDeployConfig(const std::string& filename, BaseDeployConfig& baseConfig) {
	// 读取 YAML 配置文件
	YAML::Node root = YAML::LoadFile(filename);

	// 解析 Etcd 配置
	for (const auto& host : root["Etcd"]["Hosts"]) {
		baseConfig.add_etcd_hosts(host.as<std::string>());
	}

	// 可选解析：节点续约间隔
	if (root["Etcd"]["KeepaliveInterval"]) {
		baseConfig.set_lease_renew_interval(root["Etcd"]["KeepaliveInterval"].as<uint32_t>());
	}

	// 解析日志级别配置 (uint32 类型)
	if (root["LogLevel"]) {
		baseConfig.set_log_level(root["LogLevel"].as<uint32_t>());
	}

	// 解析服务列表
	for (const auto& service : root["services"]) {
		auto* s = baseConfig.add_services();
		s->set_name(service["name"].as<std::string>());
		s->set_url(service["url"].as<std::string>());
	}

	// 解析服务前缀列表
	if (root["service_discovery_prefixes"]) {
		for (const auto& prefix : root["service_discovery_prefixes"]) {
			baseConfig.mutable_service_discovery_prefixes()->Add(prefix.as<std::string>());
		}
	}

	// 解析单独的 deployservice_prefix 字段
	if (root["deployservice_prefix"]) {
		baseConfig.set_deployservice_prefix(root["deployservice_prefix"].as<std::string>());
	}

	return true;
}


// 读取游戏配置
bool readGameConfig(const std::string& filename, GameConfig& gameConfig) {
	// 读取 YAML 配置文件
	YAML::Node root = YAML::LoadFile(filename);

	// 解析场景节点类型 (uint32 类型)
	if (root["SceneNodeType"]) {
		gameConfig.set_scene_node_type(root["SceneNodeType"].as<uint32_t>());
	}

	// 解析区域ID (uint32 类型)
	if (root["ZoneId"]) {
		gameConfig.set_zone_id(root["ZoneId"].as<uint32_t>());
	}

	return true;
}
