#pragma once
#include <memory>
#include <unordered_map>
#include <entt/src/entt/entity/entity.hpp>
#include "google/protobuf/service.h"

class PlayerService
{
public:
	PlayerService(std::unique_ptr<::google::protobuf::Service>&& pb_service)
		: pb_service_(std::move(pb_service))
	{

	}

    // 玩家请求拒绝码的唯一写入规则:handler 直接写 response->mutable_error_message(),
    // 参数也在同处 add_parameters;成功不创建该字段。不得使用进程级或 TLS tip 暂存。
    // response 由 ProcessClientPlayerMessage / InvokePlayerService / SendMessageToPlayer 按请求创建,
    // 序列化后释放,本身就是请求级结果的所有者。RpcDoneCallback 延后完成同样遵守此规则。
    // 非 Empty 应答必须有 TipInfoMessage error_message,由生成层的编译期断言守住;
    // 生成的 CallMethod 只派发与检查类型契约,不搬运错误码。
    // Empty 没有 error_message 字段,失败由 PlayerTipSystem::SendToPlayer 或专用 S2C 通知传达
    // (例如 NotifySyncDigest 的 UNAVAILABLE)。
    virtual void CallMethod(const ::google::protobuf::MethodDescriptor* method,
        entt::entity player,
        const ::google::protobuf::Message* request,
        ::google::protobuf::Message* response) = 0;

    ::google::protobuf::Service* service() { return pb_service_.get(); }
private:
    std::unique_ptr<::google::protobuf::Service> pb_service_;
};

void InitPlayerService();

extern std::unordered_map<std::string, std::unique_ptr<PlayerService>> gPlayerService;


