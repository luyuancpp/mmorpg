#pragma once

#include <memory>

#include "src/redis_client/redis_client.h"

class RedisSystem
{
public:
    using HiredisPtr = std::unique_ptr<hiredis::Hiredis>;
    void Init(muduo::net::InetAddress& server_addr);
private:
    HiredisPtr muduo_hiredis_;
};


