#include "node.h"

#include <grpcpp/create_channel.h>

#include "all_config.h"
#include "game_config/deploy_json.h"
#include "log/constants/log_constants.h"
#include "log/system/console_log_system.h"
#include "logic/constants/node.pb.h"
#include "logic/event/server_event.pb.h"
#include "muduo/base/TimeZone.h"
#include "network/network_constants.h"
#include "network/rpc_session.h"
#include "proto/common/deploy_service.grpc.pb.h"
#include "service_info/service_info.h"
#include "time/system/time_system.h"

Node::Node(muduo::net::EventLoop* loop, const std::string& logFilePath)
    : loop_(loop),
    muduoLog(logFilePath, kMaxLogFileRollSize, 1) {
}

Node::~Node() {
    ShutdownNode();
}

void Node::Init() {
    InitializeLaunchTime();
    InitializeNodeConfig();
    InitTimeZone();
    InitLog();
    InitializeGameConfig();
    InitializeGrpcNode();
    InitializeSystemBeforeConnection();
    InitializeNodeFromRequestInfo();
    InitMessageInfo();

    void InitGrpcDeploySercieResponseHandler();
    InitGrpcDeploySercieResponseHandler();
}

void Node::StartRpcServer(const nodes_info_data& data)
{
    deployRpcTimer.Cancel();

    tls.dispatcher.trigger<OnServerStart>();
}

void Node::ShutdownNode() {
    muduoLog.stop();
    ReleaseNodeId();
}

void Node::InitLog() {
    muduo::Logger::setLogLevel(static_cast <muduo::Logger::LogLevel> (
    ZoneConfig::GetSingleton().ConfigInfo().loglevel()));
    
    muduo::Logger::setOutput(AsyncOutput);
    muduoLog.start();
}

void Node::InitializeNodeConfig() {
    ZoneConfig::GetSingleton().Load("game.json");
    DeployConfig::GetSingleton().Load("deploy.json");
}

void Node::InitializeGameConfig() {
    LoadAllConfig();
    LoadAllConfigAsyncWhenServerLaunch();
}

void Node::OnConfigLoadSuccessful(){
}

void Node::InitTimeZone() {
    const muduo::TimeZone tz("zoneinfo/Asia/Hong_Kong");
    muduo::Logger::setTimeZone(tz);
}

void Node::InitializeGrpcNode() {
    InitDeployServiceCompletedQueue(tls.grpc_node_registry, GlobalGrpcNodeEntity());
}

void Node::InitializeLaunchTime()
{
    node_info_.set_launch_time(TimeUtil::NowSecondsUTC());
}

void Node::InitializeNodeFromRequestInfo() {
    const auto& deploy_info = DeployConfig::GetSingleton().DeployInfo();
    const std::string targetStr = deploy_info.ip() + ":" + std::to_string(deploy_info.port());
    tls.grpc_node_registry.emplace<GrpcDeployServiceStubPtr>(GlobalGrpcNodeEntity())
        = DeployService::NewStub(grpc::CreateChannel(targetStr, grpc::InsecureChannelCredentials()));

    deployRpcTimer.RunEvery(0.001, []() {
        HandleDeployServiceCompletedQueueMessage(tls.grpc_node_registry);
        });

    NodeInfoRequest request;
    request.set_node_type(GetNodeType());  // 这是父类基类，不知道具体类型，需要子类重载
    request.set_zone_id(ZoneConfig::GetSingleton().ConfigInfo().zone_id());
    DeployServiceGetNodeInfo(tls.grpc_node_registry, GlobalGrpcNodeEntity(), request);

    renewNodeLeaseTimer.RunEvery(kRenewLeaseTime, [this]() {
        RenewLeaseIDRequest request;
        request.set_lease_id(GetNodeInfo().lease_id());
        DeployServiceRenewLease(tls.grpc_node_registry, GlobalGrpcNodeEntity(), request);
        });
}

void Node::ConnectToCentreHelper(::google::protobuf::Service* service) {
    for (auto& centre_node_info : nodesInfo.centre_info().centre_info()) {
        entt::entity id{ centre_node_info.id() };
        const auto centre_node_id = tls.centreNodeRegistry.create(id);
        if (centre_node_id != id) {
            continue;  // 如果创建失败则跳过
        }

        InetAddress centre_addr(centre_node_info.ip(), centre_node_info.port());
        auto& centre_node = tls.centreNodeRegistry.emplace<RpcClientPtr>(centre_node_id,
            std::make_shared<RpcClientPtr::element_type>(loop_, centre_addr));

        // 注册服务
        centre_node->registerService(service);
        centre_node->connect();

        // 如果需要，进行区域判断（如果有）
        if (centre_node_info.zone_id() == ZoneConfig::GetSingleton().ConfigInfo().zone_id()) {
            zoneCentreNode = centre_node;
        }
    }
}

void Node::ReleaseNodeId() {
    ReleaseIDRequest request;
    request.set_id(GetNodeId());
    request.set_node_type(GetNodeType());
    DeployServiceReleaseID(tls.grpc_node_registry, GlobalGrpcNodeEntity(), request);
}

void Node::AsyncOutput(const char* msg, int len) {
    logger().append(msg, len);
#ifdef WIN32
    Log2Console(msg, len);
#endif
}
