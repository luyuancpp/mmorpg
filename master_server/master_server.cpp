#include "master_server.h"

namespace master
{

MasterServer::MasterServer(muduo::net::EventLoop* loop,
    const muduo::net::InetAddress& listen_addr)
    : server_(loop, listen_addr),
    redis_(std::make_shared<common::RedisClient>())
{
    redis_->Connect(listen_addr.toIp(), 6379, 1, 1);
}    

void MasterServer::Start()
{
    server_.start();
}

void MasterServer::RegisterService(::google::protobuf::Service* service)
{
    server_.registerService(service);
}
}//namespace master