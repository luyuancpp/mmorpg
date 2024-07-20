#include "common_proto/db_service.pb.h"
#include "muduo/net/TcpConnection.h"
using namespace muduo;
using namespace muduo::net;

void OnAccountDBServiceLoad2RedisRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<LoadAccountResponse>& replied, Timestamp timestamp);

void OnAccountDBServiceSave2RedisRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<SaveAccountResponse>& replied, Timestamp timestamp);

