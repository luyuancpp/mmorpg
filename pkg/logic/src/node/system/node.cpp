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
    // 构造函数，初始化节点的事件循环和日志系统
}

Node::~Node() {
    ShutdownNode();  // 节点销毁时进行清理
}

void Node::Initialize() {
    InitializeLaunchTime();            // 初始化启动时间
    LoadConfigurations();              // 加载配置
    InitializeTimeZone();              // 初始化时区
    SetupLogging();                    // 设置日志系统
    InitializeGrpcServices();          // 初始化 gRPC 服务
    PrepareForBeforeConnection();            // 准备连接前的工作
    InitializeNodeFromRequestInfo();   // 从请求中初始化节点信息
    SetupMessageHandlers();            // 设置消息处理器
}

void Node::StartRpcServer(const nodes_info_data& data) {
    deployRpcTimer.Cancel();  // 取消定时器，避免重复触发
    tls.dispatcher.trigger<OnServerStart>();  // 启动服务器
}

void Node::ShutdownNode() {
    muduoLog.stop();  // 停止日志
    ReleaseNodeId();  // 释放节点 ID
}

void Node::SetupLogging() {
    muduo::Logger::setLogLevel(static_cast<muduo::Logger::LogLevel>(ZoneConfig::GetSingleton().ConfigInfo().loglevel()));
    muduo::Logger::setOutput(AsyncOutput);
    muduoLog.start();  // 启动日志
}

void Node::LoadConfigurations() {
    ZoneConfig::GetSingleton().Load("game.json");  // 加载游戏配置
    DeployConfig::GetSingleton().Load("deploy.json");  // 加载部署配置

    LoadAllConfig();
    LoadAllConfigAsyncWhenServerLaunch();
    OnConfigLoadSuccessful();
}

void Node::InitializeTimeZone() {
    const muduo::TimeZone tz("zoneinfo/Asia/Hong_Kong");
    muduo::Logger::setTimeZone(tz);  // 设置时区为香港
}

void Node::InitializeGrpcServices() {
    // 初始化 gRPC 部署服务
    InitDeployServiceCompletedQueue(tls.grpc_node_registry, GlobalGrpcNodeEntity());
}

void Node::InitializeLaunchTime() {
    node_info_.set_launch_time(TimeUtil::NowSecondsUTC());  // 记录节点的启动时间
}

void Node::InitializeNodeFromRequestInfo() {
    const auto& deploy_info = DeployConfig::GetSingleton().DeployInfo();
    const std::string targetStr = deploy_info.ip() + ":" + std::to_string(deploy_info.port());

    // 创建 gRPC 通道并初始化 gRPC 客户端
    tls.grpc_node_registry.emplace<GrpcDeployServiceStubPtr>(GlobalGrpcNodeEntity())
        = DeployService::NewStub(grpc::CreateChannel(targetStr, grpc::InsecureChannelCredentials()));

    // 定时处理部署服务的完成队列
    deployRpcTimer.RunEvery(0.001, []() {
        HandleDeployServiceCompletedQueueMessage(tls.grpc_node_registry);
    });

    // 创建请求并获取节点信息
    NodeInfoRequest request;
    request.set_node_type(GetNodeType());  // 使用子类实现具体类型
    request.set_zone_id(ZoneConfig::GetSingleton().ConfigInfo().zone_id());
    DeployServiceGetNodeInfo(tls.grpc_node_registry, GlobalGrpcNodeEntity(), request);

    // 定时更新节点租约
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
            continue;  // 如果创建失败，则跳过该中心节点
        }

        InetAddress centre_addr(centre_node_info.ip(), centre_node_info.port());
        auto& centre_node = tls.centreNodeRegistry.emplace<RpcClientPtr>(centre_node_id,
            std::make_shared<RpcClientPtr::element_type>(loop_, centre_addr));

        // 注册服务并连接
        centre_node->registerService(service);
        centre_node->connect();

        // 判断是否为当前区域的中心节点
        if (centre_node_info.zone_id() == ZoneConfig::GetSingleton().ConfigInfo().zone_id()) {
            zoneCentreNode = centre_node;
        }
    }
}

void Node::ReleaseNodeId() {
    ReleaseIDRequest request;
    request.set_id(GetNodeId());  // 获取节点 ID
    request.set_node_type(GetNodeType());  // 获取节点类型
    DeployServiceReleaseID(tls.grpc_node_registry, GlobalGrpcNodeEntity(), request);  // 释放节点 ID
}

void Node::SetupMessageHandlers()
{
    InitMessageInfo();

    void InitGrpcDeploySercieResponseHandler();
    InitGrpcDeploySercieResponseHandler();
}

void Node::AsyncOutput(const char* msg, int len) {
    logger().append(msg, len);  // 异步输出日志
#ifdef WIN32
    Log2Console(msg, len);  // 在 Windows 系统上输出到控制台
#endif
}
