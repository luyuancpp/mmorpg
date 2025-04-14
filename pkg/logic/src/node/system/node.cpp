#include "node.h"

#include <grpcpp/create_channel.h>

#include "all_config.h"
#include "config_loader/config.h"
#include "google/protobuf/util/json_util.h"
#include "grpc/generator/proto/common/deploy_service_grpc.h"
#include "grpc/generator/proto/etcd/etcd_grpc.h"
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
#include "util/network_utils.h"
#include "node/comp/node_comp.h"
#include "node/system/node_system.h"
#include "network/process_info.h"
#include "etcd_helper.h"
#include "grpc_client_system.h"

Node::Node(muduo::net::EventLoop* loop, const std::string& logFilePath)
    : loop_(loop),
      muduoLog(logFilePath, kMaxLogFileRollSize, 1) {
    // 构造函数，初始化节点的事件循环和日志系统
}

Node::~Node() {
    ShutdownNode();  // 节点销毁时进行清理
}

void Node::InitDeployService(const std::string& service_address)
{
	tls.globalNodeRegistry.emplace<GrpcDeployServiceStubPtr>(GlobalGrpcNodeEntity())
		= DeployService::NewStub(grpc::CreateChannel(service_address, grpc::InsecureChannelCredentials()));

	// 创建请求并获取节点信息
	NodeInfoRequest request;
	request.set_node_type(GetNodeType());  // 使用子类实现具体类型
	SendDeployServiceGetNodeInfo(tls.globalNodeRegistry, GlobalGrpcNodeEntity(), request);

	// 定时更新节点租约
	GetRenewNodeLeaseTimer().RunEvery(kRenewLeaseTime, [this]() {
		RenewLeaseIDRequest request;
		request.set_lease_id(GetNodeInfo().lease_id());
		SendDeployServiceRenewLease(tls.globalNodeRegistry, GlobalGrpcNodeEntity(), request);
		});
}

void Node::Initialize() {
	LoadConfiguration();
	SetupEnvironment();
	InitGrpcClients();
	InitGrpcQueues();
	FetchServiceRegistry();
	RegisterSelf();
	SetupEventHandlers();
}

void Node::InitializeMiscellaneous() {
    GetNodeInfo().set_launch_time(TimeUtil::NowSecondsUTC());  // 记录节点的启动时间
    GetNodeInfo().set_scene_node_type(tlsCommonLogic.GetGameConfig().scene_node_type());
    GetNodeInfo().set_node_type(GetNodeType());
    GetNodeInfo().set_zone_id(tlsCommonLogic.GetGameConfig().zone_id());

	tls.globalNodeRegistry.emplace<ServiceNodeList>(GlobalGrpcNodeEntity());   
}

void Node::StartRpcServer() {
	InetAddress service_addr(GetNodeInfo().endpoint().ip(), GetNodeInfo().endpoint().port());
	rpcServer = std::make_unique<RpcServerPtr::element_type>(loop_, service_addr);
	rpcServer->start();

    tls.dispatcher.trigger<OnServerStart>();  // 启动服务器

	deployQueueTimer.Cancel();

	StartWatchingServices();

	RegisterSelf();
}


//优雅关闭和资源释放
void Node::ShutdownNode() {

    StopWatchingAll();
	tls.dispatcher.clear();  // 清除所有事件处理器
    muduoLog.stop();
    ReleaseNodeId();

    // 取消所有定时任务
    deployQueueTimer.Cancel();
    renewNodeLeaseTimer.Cancel();
	etcdQueueTimer.Cancel();

    // 关闭所有 gRPC 连接等
    
}


void Node::SetupLogging() {
    muduo::Logger::setLogLevel(static_cast<muduo::Logger::LogLevel>(tlsCommonLogic.GetBaseDeployConfig().log_level()));
    muduo::Logger::setOutput(AsyncOutput);
    muduoLog.start();  // 启动日志
}

void Node::LoadConfiguration() {
	readBaseDeployConfig("etc/base_deploy_config.yaml", tlsCommonLogic.GetBaseDeployConfig());
	readGameConfig("etc/game_config.yaml", tlsCommonLogic.GetGameConfig());
	LoadAllConfig();
	LoadAllConfigAsyncWhenServerLaunch();
	OnConfigLoadSuccessful();
}

void Node::SetupEnvironment() {
    const muduo::TimeZone tz("zoneinfo/Asia/Hong_Kong");
    muduo::Logger::setTimeZone(tz);  // 设置时区为香港

	GetNodeInfo().mutable_endpoint()->set_ip(localip());
	GetNodeInfo().mutable_endpoint()->set_port(get_available_port());
}

void Node::InitGrpcQueues() {
	InitDeployServiceCompletedQueue(tls.globalNodeRegistry, GlobalGrpcNodeEntity());
	InitetcdserverpbKVCompletedQueue(tls.globalNodeRegistry, GlobalGrpcNodeEntity());
	InitetcdserverpbWatchCompletedQueue(tls.globalNodeRegistry, GlobalGrpcNodeEntity());
	InitetcdserverpbLeaseCompletedQueue(tls.globalNodeRegistry, GlobalGrpcNodeEntity());

	deployQueueTimer.RunEvery(0.001, []() {
		HandleDeployServiceCompletedQueueMessage(tls.globalNodeRegistry);
		});

	etcdQueueTimer.RunEvery(0.001, []() {
		HandleetcdserverpbKVCompletedQueueMessage(tls.globalNodeRegistry);
		HandleetcdserverpbWatchCompletedQueueMessage(tls.globalNodeRegistry);
		HandleetcdserverpbLeaseCompletedQueueMessage(tls.globalNodeRegistry);
		});
}

void Node::InitializeNodeFromRequestInfo() {

	for (auto& serviceDiscoveryPrefixes : tlsCommonLogic.GetBaseDeployConfig().service_discovery_prefixes())
	{
		GetKeyValue(serviceDiscoveryPrefixes);
	}
}

void Node::ConnectToCentreHelper(::google::protobuf::Service* service) {
    auto& serviceNodeList = tls.globalNodeRegistry.get<ServiceNodeList>(GlobalGrpcNodeEntity());

    for (auto& centreNodeInfo : serviceNodeList[kCentreNode].node_list()) {
        entt::entity id{ centreNodeInfo.node_id() };
        const auto centre_node_id = tls.centreNodeRegistry.create(id);
        if (centre_node_id != id) {
            continue;  // 如果创建失败，则跳过该中心节点
        }

        InetAddress centre_addr(centreNodeInfo.endpoint().ip(), centreNodeInfo.endpoint().port());
        auto& centre_node = tls.centreNodeRegistry.emplace<RpcClientPtr>(centre_node_id,
            std::make_shared<RpcClientPtr::element_type>(loop_, centre_addr));

        // 注册服务并连接
        centre_node->registerService(service);
        centre_node->connect();

        // 判断是否为当前区域的中心节点
        if (centreNodeInfo.zone_id() == tlsCommonLogic.GetGameConfig().zone_id()) {
            zoneCentreNode = centre_node;
        }
    }
}

void Node::ReleaseNodeId() {
    ReleaseIDRequest request;
    request.set_id(GetNodeId());  // 获取节点 ID
    request.set_node_type(GetNodeType());  // 获取节点类型
    SendDeployServiceReleaseID(tls.globalNodeRegistry, GlobalGrpcNodeEntity(), request);  // 释放节点 ID
}

void Node::SetupEventHandlers()
{
    InitMessageInfo();

    void InitGrpcDeployServiceResponseHandler();
    InitGrpcDeployServiceResponseHandler();

    void InitGrpcetcdserverpbResponseHandler();
    InitGrpcetcdserverpbResponseHandler();

	void InitRepliedHandler();
	InitRepliedHandler();

}

void Node::GetKeyValue(const std::string& prefix)
{
	etcdserverpb::RangeRequest request;
	request.set_key(prefix);  // 设置查询前缀

	// 设置 range_end 为 prefix + 1
	std::string range_end = prefix;
	range_end[range_end.size() - 1] = range_end[range_end.size() - 1] + 1; // 将最后一个字符加 1
	request.set_range_end(range_end);  // 设置 range_end

    SendetcdserverpbKVRange(tls.globalNodeRegistry, GlobalGrpcNodeEntity(), request);
}

void Node::FetchServiceRegistry() {
	for (const auto& prefix : tlsCommonLogic.GetBaseDeployConfig().service_discovery_prefixes()) {
		EtcdHelper::RangeQuery(prefix);  // 拉取已有服务节点信息
	}
}

void Node::StartWatchingServices() {
	for (const auto& prefix : tlsCommonLogic.GetBaseDeployConfig().service_discovery_prefixes()) {
		WatchPrefix(prefix);  // 开始监听注册/变更
	}
}

void Node::StartWatchingPrefix(const std::string& prefix)
{
	etcdserverpb::WatchRequest request;
    auto& createRequest = *request.mutable_create_request();

    createRequest.set_key(prefix);  // 设置查询前缀

	// 设置 range_end 为 prefix + 1
	std::string range_end = prefix;
	range_end[range_end.size() - 1] = range_end[range_end.size() - 1] + 1; // 将最后一个字符加 1
    createRequest.set_range_end(range_end);  // 设置 range_end

    SendetcdserverpbWatchWatch(tls.globalNodeRegistry, GlobalGrpcNodeEntity(), request);
}

void Node::StopWatchingAll()
{
	EtcdHelper::StopAllWatching();
}

void Node::RegisterSelf() {
	GetNodeInfo().set_launch_time(TimeUtil::NowSecondsUTC());
	GetNodeInfo().set_scene_node_type(tlsCommonLogic.GetGameConfig().scene_node_type());
	GetNodeInfo().set_node_type(GetNodeType());
	GetNodeInfo().set_zone_id(tlsCommonLogic.GetGameConfig().zone_id());

	EtcdHelper::PutServiceNodeInfo(GetNodeInfo(), GetServiceName());
}

void Node::AsyncOutput(const char* msg, int len) {
    logger().append(msg, len);
#ifdef WIN32
    Log2Console(msg, len);  // 在 Windows 系统上输出到控制台
#endif
}

void Node::InitGrpcClients() {
	GrpcClientSystem::InitEtcdStubs(tlsCommonLogic.GetBaseDeployConfig().etcd_hosts());
}


std::string Node::FormatIpAndPort()
{
	return ::FormatIpAndPort(GetNodeInfo().endpoint().ip(), GetNodeInfo().endpoint().port());
}

std::string Node::GetIp() 
{
	return GetNodeInfo().endpoint().ip();
}

uint32_t Node::GetPort() 
{
	return GetNodeInfo().endpoint().port();
}

bool Node::ParseJsonToServiceNode(const std::string& jsonValue, uint32_t serviceNodeType) {
	auto& serviceNodeList = tls.globalNodeRegistry.get<ServiceNodeList>(GlobalGrpcNodeEntity());

	// 检查 serviceNodeType 是否有效
	if (!eNodeType_IsValid(serviceNodeType)) {
		return false;
	}

	// 调用 JsonStringToMessage 函数将 JSON 字符串解析到 protobuf 消息
	auto result = google::protobuf::util::JsonStringToMessage(jsonValue, serviceNodeList[serviceNodeType].add_node_list());

	if (!result.ok()) {
		// 解析失败时记录错误日志
		LOG_ERROR << "Failed to parse JSON to message for serviceNodeType: " << serviceNodeType
			<< ", JSON: " << jsonValue
			<< ", Error: " << result.message().data();
		return false;
	}

	return true;  // 解析成功
}

// 处理服务节点的逻辑
void Node::HandleServiceNode(const std::string& key, const std::string& value) {

	// 获取服务节点类型
	auto serviceNodeType = NodeSystem::GetServiceTypeFromPrefix(key);

	if (serviceNodeType == kDeployNode) {
		// 处理部署服务
		InitDeployService(value);
		LOG_INFO << "Deploy Service Key: " << key << ", Value: " << value;
	}
	else if (serviceNodeType == kLoginNode) {

	}
	else if (eNodeType_IsValid(serviceNodeType)) {
		// 解析有效的服务节点
		if (ParseJsonToServiceNode(value, serviceNodeType)) {
			LOG_INFO << "Service Node Key: " << key << ", Value: " << value;
		}
		else {
			LOG_ERROR << "Failed to parse JSON for Service Node Key: " << key << ", Value: " << value;
		}
	}
	else {
		// 无效的服务类型
		LOG_ERROR << "Unknown service type for key: " << key;
	}
}


void Node::WatchPrefix(const std::string& prefix) {
	EtcdHelper::StartWatchingPrefix(prefix);
}