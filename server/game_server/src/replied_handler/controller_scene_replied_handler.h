#include "logic_proto/controller_scene.pb.h"
#include "muduo/net/TcpConnection.h"
using namespace muduo;
using namespace muduo::net;

void OnControllerSceneServiceUpdateCrossMainSceneInfoRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<UpdateCrossMainSceneInfoResponse>& replied, Timestamp timestamp);

