#include "logic_proto/centre_scene.pb.h"
#include "muduo/net/TcpConnection.h"
using namespace muduo;
using namespace muduo::net;

void OnCentreSceneServiceUpdateCrossMainSceneInfoRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<UpdateCrossMainSceneInfoResponse>& replied, Timestamp timestamp);

