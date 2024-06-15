#include "common_proto/deploy_service.pb.h"
#include "muduo/net/TcpConnection.h"
using namespace muduo;
using namespace muduo::net;

void OnDeployServiceGetNodeInfoRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<NodeInfoResponse>& replied, Timestamp timestamp);

