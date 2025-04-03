#include "node.h"

#include <grpcpp/create_channel.h>

#include "all_config.h"
#include "config_loader/config.h"
#include "grpc/generator/deploy_service_grpc.h"
#include "grpc/generator/etcd_grpc.h"
#include "log/constants/log_constants.h"
#include "log/system/console_log_system.h"
#include "logic/constants/node.pb.h"
#include "logic/event/server_event.pb.h"
#include "muduo/base/TimeZone.h"
#include "network/network_constants.h"
#include "network/rpc_session.h"
#include "proto/common/deploy_service.grpc.pb.h"
#include "service_info/service_info.h"
#include "thread_local/storage_common_logic.h"
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


//优雅关闭和资源释放
void Node::ShutdownNode() {
    // 停止日志系统
    muduoLog.stop();

    // 释放节点 ID
    ReleaseNodeId();

    // 取消所有定时任务
    deployRpcTimer.Cancel();
    renewNodeLeaseTimer.Cancel();

    // 关闭所有 gRPC 连接等
    
}


void Node::SetupLogging() {
    muduo::Logger::setLogLevel(static_cast<muduo::Logger::LogLevel>(tlsCommonLogic.GetBaseDeployConfig().log_level()));
    muduo::Logger::setOutput(AsyncOutput);
    muduoLog.start();  // 启动日志
}

void Node::LoadConfigurations() {
    readBaseDeployConfig("etc/base_deploy_config.yaml", tlsCommonLogic.GetBaseDeployConfig());
    readGameConfig("etc/game_config.yaml", tlsCommonLogic.GetGameConfig());

    LoadAllConfig();
    LoadAllConfigAsyncWhenServerLaunch();
    OnConfigLoadSuccessful();
}

void Node::InitializeTimeZone() {
    const muduo::TimeZone tz("zoneinfo/Asia/Hong_Kong");
    muduo::Logger::setTimeZone(tz);  // 设置时区为香港
}

void Node::InitializeGrpcServices() {
    InitDeployServiceCompletedQueue(tls.grpc_node_registry, GlobalGrpcNodeEntity());
    InitetcdserverpbKVCompletedQueue(tls.grpc_node_registry, GlobalGrpcNodeEntity());
}

void Node::InitializeLaunchTime() {
    node_info_.set_launch_time(TimeUtil::NowSecondsUTC());  // 记录节点的启动时间
}

void Node::InitializeNodeFromRequestInfo() {
    //todo const auto& deploy_info = DeployConfig::GetSingleton().DeployInfo();
    auto& targetStr = tlsCommonLogic.GetBaseDeployConfig().etcd_hosts();

    try {
        // 创建 gRPC 通道并初始化 gRPC 客户端
        tls.grpc_node_registry.emplace<GrpcDeployServiceStubPtr>(GlobalGrpcNodeEntity())
            = DeployService::NewStub(grpc::CreateChannel("127.0.0.1:1000", grpc::InsecureChannelCredentials()));

        tls.grpc_node_registry.emplace<GrpcetcdserverpbKVStubPtr>(GlobalGrpcNodeEntity())
            = etcdserverpb::KV::NewStub(grpc::CreateChannel("127.0.0.1:2379", grpc::InsecureChannelCredentials()));

    }
    catch (const std::exception& e) {
       LOG_FATAL <<  "Failed to initialize gRPC service: " << std::string(e.what());
    }

    // 定时处理部署服务的完成队列
    deployRpcTimer.RunEvery(0.001, []() {
        HandleDeployServiceCompletedQueueMessage(tls.grpc_node_registry);
        HandleetcdserverpbKVCompletedQueueMessage(tls.grpc_node_registry);
    });

    {
        etcdserverpb::RangeRequest request;
		std::string prefix = "deployservice.rpc";  // 设置查询前缀
        request.set_key(prefix);  // 设置查询前缀
		// 设置 range_end 为 prefix + 1
		std::string range_end = prefix;
		range_end[range_end.size() - 1] = range_end[range_end.size() - 1] + 1; // 将最后一个字符加 1
		request.set_range_end(range_end);  // 设置 range_end

        etcdserverpbKVRange(tls.grpc_node_registry, GlobalGrpcNodeEntity(), request);
    }
    

    // 创建请求并获取节点信息
    NodeInfoRequest request;
    request.set_node_type(GetNodeType());  // 使用子类实现具体类型
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
        if (centre_node_info.zone_id() == tlsCommonLogic.GetGameConfig().zone_id()) {
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

    void InitGrpcDeployServiceResponseHandler();
    InitGrpcDeployServiceResponseHandler();

    void InitGrpcetcdserverpbKVResponseHandler();
    InitGrpcetcdserverpbKVResponseHandler();
}

void Node::AsyncOutput(const char* msg, int len) {
    logger().append(msg, len);
#ifdef WIN32
    Log2Console(msg, len);  // 在 Windows 系统上输出到控制台
#endif
}
