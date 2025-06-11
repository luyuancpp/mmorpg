#include "config.h"
#include <yaml-cpp/yaml.h>
#include <muduo/base/Logging.h>

// 读取基础部署配置
bool readBaseDeployConfig(const std::string& filename, BaseDeployConfig& baseConfig) {
	// 读取 YAML 配置文件
	YAML::Node root = YAML::LoadFile(filename);

	// 解析 Etcd 配置
	if (root["Etcd"]["Hosts"]) {
		for (const auto& host : root["Etcd"]["Hosts"]) {
			baseConfig.add_etcd_hosts(host.as<std::string>());
		}
	}

	// 节点续约间隔
	if (root["Etcd"]["KeepaliveInterval"]) {
		baseConfig.set_keep_alive_interval(root["Etcd"]["KeepaliveInterval"].as<uint32_t>());
	}

	// 日志级别配置 (uint32 类型)
	if (root["LogLevel"]) {
		baseConfig.set_log_level(root["LogLevel"].as<uint32_t>());
	}

	// 服务列表
	if (root["services"]) {
		for (const auto& service : root["services"]) {
			auto* s = baseConfig.add_services();
			s->set_name(service["name"].as<std::string>());
			s->set_url(service["url"].as<std::string>());
		}
	}

	// 服务前缀列表
	if (root["service_discovery_prefixes"]) {
		for (const auto& prefix : root["service_discovery_prefixes"]) {
			baseConfig.mutable_service_discovery_prefixes()->Add(prefix.as<std::string>());
		}
	}

	// 单独的 deployservice_prefix 字段
	if (root["deployservice_prefix"]) {
		baseConfig.set_deployservice_prefix(root["deployservice_prefix"].as<std::string>());
	}

	// 节点 TTL 配置
	if (root["Etcd"]["NodeTTLSeconds"]) {
		baseConfig.set_node_ttl_seconds(root["Etcd"]["NodeTTLSeconds"].as<uint32_t>());
	}

	// 新增：健康检查间隔
	if (root["HealthCheckInterval"]) {
		baseConfig.set_health_check_interval(root["HealthCheckInterval"].as<uint32_t>());
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

	// 解析 zoneredis 配置
	if (root["zoneredis"]) {
		YAML::Node zoneredisNode = root["zoneredis"];

		// 检查并解析 Redis 配置
		if (zoneredisNode["host"]) {
			gameConfig.mutable_zone_redis()->set_host(zoneredisNode["host"].as<std::string>());
		}
		if (zoneredisNode["port"]) {
			gameConfig.mutable_zone_redis()->set_port(zoneredisNode["port"].as<uint32_t>());
		}
		if (zoneredisNode["password"]) {
			gameConfig.mutable_zone_redis()->set_password(zoneredisNode["password"].as<std::string>());
		}
		if (zoneredisNode["db"]) {
			gameConfig.mutable_zone_redis()->set_db(zoneredisNode["db"].as<uint32_t>());
		}
	}
	else {
		LOG_ERROR << "Warning: zoneredis configuration not found in config file.";
		return false;
	}

	return true;
}
