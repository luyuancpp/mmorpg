#pragma once

#include <string>
#include <memory>
#include <vector>

#include "deps/hiredis/hiredis.h"

#include "src/common_type/common_type.h"

namespace google
{
    namespace protobuf
    {
        class Message;
    }//namespace protobuf
}// namespace google

class PbSyncRedisClient
{
public:
    using ContextPtr = std::shared_ptr<redisContext>;
    using ReplyPtr = std::unique_ptr<redisReply>;
    using MessageCachedArray = std::vector<uint8_t>;

    void Connect(const std::string& redis_server_addr, int32_t port, int32_t sec, int32_t usec);

    void Save(const google::protobuf::Message& message);
    void Save(const google::protobuf::Message& message, Guid guid);
    void Save(const google::protobuf::Message& message, const std::string& key);

    void Load(google::protobuf::Message& message);
    void Load(google::protobuf::Message& message, Guid guid);
    void Load(google::protobuf::Message& message, const std::string& key);
private:
    void OnDisconnect();
   
    ContextPtr context_;
};

using PbSyncRedisClientPtr = std::shared_ptr<PbSyncRedisClient>;

