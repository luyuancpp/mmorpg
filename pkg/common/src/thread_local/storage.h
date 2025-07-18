#pragma once

#include "util/game_registry.h"
#include "util/snow_flake.h"

#include "proto/common/node.pb.h"

#include <kafka/kafka_consumer.h>
#include <kafka/kafka_producer.h>

class ThreadLocalStorage
{
public:
	using NodeRgistries = std::array<entt::registry, eNodeType_ARRAYSIZE>;
	using NodeGloabalEntity = std::array<entt::entity, eNodeType_ARRAYSIZE>;
	using KafkaProducerPtr = std::unique_ptr<KafkaProducer>;
	using KafkaConsumerPtr = std::unique_ptr<KafkaConsumer>;

	ThreadLocalStorage();

	entt::registry globalRegistry;
	entt::registry actorRegistry;
	entt::registry sceneRegistry;
	entt::registry itemRegistry;
	entt::registry sessionRegistry;
	entt::registry nodeGlobalRegistry;

	entt::dispatcher dispatcher;

	entt::entity operatorEntity{ entt::null };
	entt::entity errorEntity{ entt::null };
	entt::entity globalEntity{ entt::null };
	entt::entity globalGrpcNodeEntity{ entt::null };

	SnowFlake itemIdGenerator;
	Guid lastGeneratorItemGuid{ kInvalidGuid };

	void Clear();

	entt::registry& GetNodeRegistry(uint32_t nodeType) {
		return nodeRegistries[nodeType];
	}

	NodeRgistries& GetNodeRegistry() {
		return nodeRegistries;
	}

	entt::entity GetNodeGlobalEntity(uint32_t nodeType);


	void OnNodeStart(uint32_t nodeId);

	KafkaProducerPtr& GetKafkaProducer() { return kafkaProducer; }
	KafkaConsumerPtr& GetKafkaConsumer() { return kafkaConsumer; }
private:
	NodeRgistries nodeRegistries;
	NodeGloabalEntity nodeGlobalEntities;
	KafkaProducerPtr kafkaProducer;
	KafkaConsumerPtr kafkaConsumer;
};

extern thread_local ThreadLocalStorage tls;