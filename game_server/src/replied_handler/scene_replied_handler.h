#include "logic_proto/scene.pb.h"
#include "muduo/net/TcpConnection.h"
using namespace muduo;
using namespace muduo::net;

void OnServerSceneServiceUpdateCrossMainSceneInfoRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<UpdateCrossMainSceneInfoResponse>& replied, Timestamp timestamp);

