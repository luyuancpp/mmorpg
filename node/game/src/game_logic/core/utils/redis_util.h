#pragma once

#include <memory>

#include "redis_client/redis_client.h"

class RedisUtil
{
public:
    using HiredisPtr = std::unique_ptr<hiredis::Hiredis>;
    void Initialize(muduo::net::InetAddress& server_addr);
private:
    HiredisPtr hiredis;
};


