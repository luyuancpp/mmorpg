#include "logic_proto/lobby_scene.pb.h"
#include "muduo/net/TcpConnection.h"
using namespace muduo;
using namespace muduo::net;

void OnStartCrossGsRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<StartCrossGsResponse>& replied, Timestamp timestamp);

void OnEnterCrossMainSceneRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<EnterCrossMainSceneResponese>& replied, Timestamp timestamp);

void OnEnterCrossMainSceneWeightRoundRobinRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<EnterCrossRoomSceneSceneWeightRoundRobinResponse>& replied, Timestamp timestamp);

