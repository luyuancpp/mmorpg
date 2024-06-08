#include "common_proto/deploy_service.pb.h"
#include "muduo/net/TcpConnection.h"
using namespace muduo;
using namespace muduo::net;

void OnDeployServiceGetNodeInfoRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<NodeInfoResponse>& replied, Timestamp timestamp);

void OnDeployServiceStartGsRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<StartGsResponse>& replied, Timestamp timestamp);

void OnDeployServiceGetGsNodeIdRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<GsNodeIdResponse>& replied, Timestamp timestamp);

