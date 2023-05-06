#include "logic_proto/team.pb.h"
#include "muduo/net/TcpConnection.h"
using namespace muduo;
using namespace muduo::net;

void OnEnterSceneRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<TeamTestS2SRespone>& replied, Timestamp timestamp);

