#include "common_proto/deploy_service.pb.h"
#include "muduo/net/TcpConnection.h"
using namespace muduo;
using namespace muduo::net;

void OnDeployServiceServerInfoRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<ServerInfoResponse>& replied, Timestamp timestamp);

void OnDeployServiceStartGSRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<StartGSResponse>& replied, Timestamp timestamp);

void OnDeployServiceStartLobbyServerRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<LobbyServerResponse>& replied, Timestamp timestamp);

void OnDeployServiceAcquireLobbyServerInfoRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<LobbyServerResponse>& replied, Timestamp timestamp);

void OnDeployServiceAcquireLobbyInfoRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<LobbyInfoResponse>& replied, Timestamp timestamp);

void OnDeployServiceLoginNodeInfoRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<GruoupLoginNodeResponse>& replied, Timestamp timestamp);

void OnDeployServiceSceneSequenceNodeIdRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<SceneSqueueResponese>& replied, Timestamp timestamp);

