#pragma once
#include <cstdint>
#include <string>

// 玩家存盘 DBTask 的 Kafka topic 名。规则与 go/db、go/login 的
// config.DbTaskTopicForGeneration 逐字一致(C++ scene 是生产者,go/db 是唯一的消费者,
// topic 名是两边唯一的会合点;规则漂移不报任何错,存盘静默写进没人消费的 topic):
//   generation <= 1 → "db_task_zone_{zoneId}"
//   generation >= 2 → "db_task_zone_{zoneId}_g{generation}"
//
// 为什么第一代**不带**后缀,而审计 topic(modules/audit/audit_topic.h)第一代就带 _g1:
// db_task_zone_{zone} 这个名字在引入世代号之前就已上线、承载着 applied cursor 的 topic namespace,
// 给第一代补后缀等于换 topic,只能沿用 Go 侧早已定下的口径。0 与 1 同义(proto3 没有 presence,
// 配置里不写就是 0)。
//
// 本函数只做纯拼接、不读配置,方便测试工程直接包;调用方从
// BaseDeployConfig.db_task_topic_generation(yaml 键 DbTaskTopicGeneration)取世代号传进来。
// 换代流程见 docs/design/db-task-kafka-partition-contract.md。
inline std::string GetDbTaskTopic(uint32_t zoneId, uint32_t generation)
{
    std::string topic = "db_task_zone_" + std::to_string(zoneId);
    if (generation <= 1)
    {
        return topic;
    }
    return topic + "_g" + std::to_string(generation);
}

// 玩家存盘 DBTask 的 Kafka key:玩家 id 的无符号十进制 ASCII,无前导零。与 Go 侧
// kafkautil.DBTaskKey(strconv.FormatUint(id, 10))逐字节相同 —— login 发读请求用的就是那一个。
// 分区 = CRC-32/IEEE(key) mod N(librdkafka consistent_random,见 kafka_producer.cpp createProducer),
// 所以 key 的字节就是分区契约的一部分:这里换一种写法(补前导零、改成有符号、加前缀),同一玩家的写
// 就会换分区,被 go/db 判为 orderingIncomparable 送进死信,读后写屏障也随之失效。
// 契约见 docs/design/db-task-kafka-partition-contract.md「分区函数契约」;
// 两侧共读的 golden 向量在 go/shared/kafkautil/testdata/dbtask_partition_vectors.json。
inline std::string DbTaskKafkaKey(uint64_t playerId)
{
    return std::to_string(playerId);
}
