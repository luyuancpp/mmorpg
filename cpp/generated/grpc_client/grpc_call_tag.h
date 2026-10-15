#pragma once
#include <chrono>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <grpcpp/client_context.h>
#include <grpcpp/support/status.h>

// 本文件不是生成器产物,手工维护:tools/proto_generator 的 grpc_async_client / grpc_init_total 模板
// 生成的代码引用这里的类型。设计见 docs/design/grpc-client-deadline-failure-callback.md。

struct GrpcTag {
	uint32_t messageId;
	void* valuePtr;
};

// C++ 节点 unary gRPC 调用的内置 deadline(毫秒)。bin/etc/base_deploy_config.yaml 的 GrpcClient.CallDeadlineMs
// 没配到的目标节点类型用它(grpc_call_deadline::Apply)。取值须大于各 Go 服务 zrpc 服务端 Timeout 的最大值
// (scene_manager 8000)加 2000 余量:下游先超时,调用方才收得到带真实错误码的应答(上游比下游宽)。
inline constexpr uint32_t kDefaultGrpcCallDeadlineMs = 10000;

// 本次调用随请求发出的 metadata,按 Send 传入的原值保存(Base64 之前)。
using GrpcSentMetadata = std::vector<std::pair<std::string, std::string>>;

// 每次调用回调(Send<Svc><Method> 的 GrpcDone 重载)拿到的结果。code 与 grpc::StatusCode 同值,0 = OK。
// 非 OK 与 GrpcCallFailure 同义:结果未知,不代表服务端没执行。
struct GrpcCallOutcome {
	uint32_t code{0};
	std::string message;

	bool ok() const { return code == 0; }
};

// 每次调用的完成回调,契约(docs/design/grpc-client-deadline-failure-callback.md「每次调用回调」一节):
// - 恰好调用一次;只在节点所属 EventLoop 线程调用,从不在 Send 栈内同步调用(本地拒绝也延后投递)。
// - reply 只在 outcome.ok() 时非空,只在回调期间有效;要留用就拷贝。
// - 带 done 的调用不再触发该方法的全局 Async<Svc><Method>Handler / FailedHandler。
// - 节点摘除 / 停机先以非 OK 完成在途调用(官方 CQ 取消为 CANCELLED,本地拒绝为 UNAVAILABLE),
//   再拆节点;回调里不要假设节点仍可用。
template <typename Reply>
using GrpcDone = std::function<void(const GrpcCallOutcome&, const Reply* reply)>;

// 每次调用的可选参数。deadline <= 0 用该 proto 文件的默认 deadline(Set<File>CallDeadline);
// metadata 填原值,发出前由生成代码做 Base64,失败时仍按原值交还(与 metaKeys/metaValues 重载一致)。
struct GrpcCallOptions {
	std::chrono::milliseconds deadline{0};
	GrpcSentMetadata metadata;
};

// 一次 unary 调用以非 OK 状态结束时交给失败处理器(Async<Svc><Method>FailedHandler)的上下文。
// 只在回调期间有效,不要保存其中的引用。
//
// 失败 = **结果未知**:DEADLINE_EXCEEDED / UNAVAILABLE / CANCELLED 都不代表服务端没执行 —— 服务端超时后
// handler 可能仍在跑完,连接也可能在请求发出之后才断。失败处理器只做不依赖对端是否执行的事(释放本地
// 在途记录、提示客户端、按退避重试幂等请求);需要裁决对端状态的,仍走各自的看门狗 / 权威数据。
//
// 为什么带上发出的 metadata:失败时服务端没有回写 initial metadata(GetServerInitialMetadata 为空),
// 应答里的回显字段也拿不到;按会话 / 请求编号认领失败的调用方(gate、号段传输)只能从这里取。
struct GrpcCallFailure {
	uint32_t messageId;                    // 与 gRpcMethodRegistry 同一编号
	const char* method;                    // "<Service>.<Method>",日志用
	const grpc::ClientContext& context;
	const grpc::Status& status;            // 非 OK
	const GrpcSentMetadata& sentMetadata;

	// 按 key 找本次发出的 metadata 原值;没有返回 nullptr。
	const std::string* FindSentMetadata(std::string_view key) const {
		for (const auto& [sentKey, sentValue] : sentMetadata) {
			if (sentKey == key) {
				return &sentValue;
			}
		}
		return nullptr;
	}
};
