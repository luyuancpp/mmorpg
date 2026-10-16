#pragma once
#include <chrono>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// 本文件不是生成器产物,手工维护:tools/proto_generator 的 grpc_async_client / grpc_init_total 模板
// 生成的代码引用这里的类型。设计见 docs/design/grpc-client-deadline-failure-callback.md 与
// docs/design/grpc-muduo-transport.md §14。
//
// 与传输无关:不含任何官方 gRPC 类型。2026-10-15 第 3 批起生成客户端走 muduo_grpc::Channel,第 5 批删除官方运行时。

// C++ 节点 unary gRPC 调用的内置 deadline(毫秒)。bin/etc/base_deploy_config.yaml 的 GrpcClient.CallDeadlineMs
// 没配到的目标节点类型用它(grpc_call_deadline::Apply)。取值须大于各 Go 服务 zrpc 服务端 Timeout 的最大值
// (scene_manager 8000)加 2000 余量:下游先超时,调用方才收得到带真实错误码的应答(上游比下游宽)。
inline constexpr uint32_t kDefaultGrpcCallDeadlineMs = 10000;

// gRPC metadata:有序的键值对。随请求发出的按 Send 传入的原值保存(应用层 Base64 之前);
// 应答里的按收到的值保存(`-bin` 键已解掉线上那一层 base64,与官方客户端的 GetServerInitialMetadata 同义)。
using GrpcMetadata = std::vector<std::pair<std::string, std::string>>;
using GrpcSentMetadata = GrpcMetadata;

// 与 grpc::StatusCode 同值(线上 grpc-status 的数值)。只列调用方需要按名字判断的几个。
inline constexpr uint32_t kGrpcStatusOk = 0;
inline constexpr uint32_t kGrpcStatusCancelled = 1;
inline constexpr uint32_t kGrpcStatusDeadlineExceeded = 4;
inline constexpr uint32_t kGrpcStatusUnimplemented = 12;
inline constexpr uint32_t kGrpcStatusInternal = 13;
inline constexpr uint32_t kGrpcStatusUnavailable = 14;

inline const std::string* FindGrpcMetadata(const GrpcMetadata& metadata, std::string_view key) {
	for (const auto& [entryKey, entryValue] : metadata) {
		if (entryKey == key) {
			return &entryValue;
		}
	}
	return nullptr;
}

// 一次 unary 调用的结果。code 与 grpc::StatusCode 同值,0 = OK。
// 非 OK 与 GrpcCallFailure 同义:结果未知,不代表服务端没执行。
struct GrpcCallOutcome {
	uint32_t code{kGrpcStatusOk};
	std::string message;
	// 服务端回写的响应头 / trailers 里的自定义 metadata;失败时可能为空(连接断开、本地拒绝)。
	GrpcMetadata initialMetadata;
	GrpcMetadata trailingMetadata;

	bool ok() const { return code == kGrpcStatusOk; }

	// 按 key 找响应头里的 metadata;没有返回 nullptr。按回显认领的调用方(gate 会话、号段请求编号)用它。
	const std::string* FindInitialMetadata(std::string_view key) const { return FindGrpcMetadata(initialMetadata, key); }
};

// 全局应答处理器(Async<Svc><Method>Handler)拿到的应答上下文。只在回调期间有效,不要保存其中的引用。
using GrpcReplyContext = GrpcCallOutcome;

// 每次调用的完成回调,契约(docs/design/grpc-client-deadline-failure-callback.md「每次调用回调」一节):
// - 恰好调用一次;只在节点所属 EventLoop 线程调用,从不在 Send 栈内同步调用(本地拒绝也延后投递)。
// - reply 只在 outcome.ok() 时非空,只在回调期间有效;要留用就拷贝。
// - 带 done 的调用不再触发该方法的全局 Async<Svc><Method>Handler / FailedHandler。
// - 节点摘除 / 停机先以非 OK 完成在途调用,再拆节点;回调里不要假设节点仍可用。摘节点时回调在实体销毁之后才执行,
//   不要捕获实体句柄或组件指针。
// - 不要按 code 区分「停机」与「传输失败」:同一停机场景旧官方 CQ 给 CANCELLED、muduo_grpc 通道给 UNAVAILABLE,
//   换传输后会变。非 OK 一律当结果未知,只释放本地在途状态。
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
// 为什么带上发出的 metadata:失败时服务端通常没有回写响应头,应答里的回显字段也拿不到;
// 按会话 / 请求编号认领失败的调用方(gate、号段传输)只能从这里取。
struct GrpcCallFailure {
	uint32_t messageId;                    // 与 gRpcMethodRegistry 同一编号
	const char* method;                    // "<Service>.<Method>",日志用
	const GrpcCallOutcome& outcome;        // 非 OK
	const GrpcSentMetadata& sentMetadata;

	uint32_t code() const { return outcome.code; }
	const std::string& message() const { return outcome.message; }

	// 按 key 找本次发出的 metadata 原值;没有返回 nullptr。
	const std::string* FindSentMetadata(std::string_view key) const { return FindGrpcMetadata(sentMetadata, key); }
};
