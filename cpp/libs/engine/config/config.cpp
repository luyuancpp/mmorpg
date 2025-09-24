#include "config.h"
#include <yaml-cpp/yaml.h>
#include <muduo/base/Logging.h>
#include <node_config_manager.h>

bool readBaseDeployConfig(const std::string& filename, BaseDeployConfig& baseConfig) {
	YAML::Node root = YAML::LoadFile(filename);

	// 解析 Etcd 配置
	if (root["Etcd"]["Hosts"]) {
		for (const auto& host : root["Etcd"]["Hosts"]) {
			baseConfig.add_etcd_hosts(host.as<std::string>());
		}
	}
	if (root["Etcd"]["KeepaliveInterval"]) {
		baseConfig.set_keep_alive_interval(root["Etcd"]["KeepaliveInterval"].as<uint32_t>());
	}
	if (root["Etcd"]["NodeTTLSeconds"]) {
		baseConfig.set_node_ttl_seconds(root["Etcd"]["NodeTTLSeconds"].as<uint32_t>());
	}

	// 日志级别
	if (root["LogLevel"]) {
		baseConfig.set_log_level(root["LogLevel"].as<uint32_t>());
	}

	// 健康检查间隔
	if (root["HealthCheckInterval"]) {
		baseConfig.set_health_check_interval(root["HealthCheckInterval"].as<uint32_t>());
	}

	// 服务列表
	if (root["services"]) {
		for (const auto& service : root["services"]) {
			auto* s = baseConfig.add_services();
			s->set_name(service["name"].as<std::string>());
			s->set_url(service["url"].as<std::string>());
		}
	}

	// 服务发现前缀
	if (root["service_discovery_prefixes"]) {
		for (const auto& prefix : root["service_discovery_prefixes"]) {
			baseConfig.add_service_discovery_prefixes(prefix.as<std::string>());
		}
	}

	// deployservice_prefix
	if (root["deployservice_prefix"]) {
		baseConfig.set_deployservice_prefix(root["deployservice_prefix"].as<std::string>());
	}

	// 读取配置表目录字段
	if (root["TableDataDirectory"]) {
		baseConfig.set_table_data_directory(root["TableDataDirectory"].as<std::string>());
	}

	// ✅ Kafka 配置读取
	if (root["Kafka"]) {
		const YAML::Node& kafkaNode = root["Kafka"];
		KafkaConfig* kafkaConfig = baseConfig.mutable_kafka();

		if (kafkaNode["Brokers"]) {
			for (const auto& broker : kafkaNode["Brokers"]) {
				kafkaConfig->add_brokers(broker.as<std::string>());
			}
		}
		if (kafkaNode["Topics"]) {
			for (const auto& topic : kafkaNode["Topics"]) {
				kafkaConfig->add_topics(topic.as<std::string>());
			}
		}
		if (kafkaNode["GroupID"]) {
			kafkaConfig->set_group_id(kafkaNode["GroupID"].as<std::string>());
		}
		if (kafkaNode["EnableAutoCommit"]) {
			kafkaConfig->set_enable_auto_commit(kafkaNode["EnableAutoCommit"].as<bool>());
		}
		if (kafkaNode["AutoOffsetReset"]) {
			kafkaConfig->set_auto_offset_reset(kafkaNode["AutoOffsetReset"].as<std::string>());
		}
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

void InitThreadLocalConfig() {
	tlsNodeConfigManager = gNodeConfigManager;
}

std::string GetConfigDir() {
	return tlsNodeConfigManager.GetBaseDeployConfig().table_data_directory();
}