#pragma once

#include <threading/registry_manager.h>

entt::entity GlobalEntity();

// 模板函数：发送错误信息给客户端
template <typename Request, typename Response>
void SendErrorToClient(const Request& request, Response& response, uint32_t err) {
    // 设置 message_id
    response.mutable_message_content()->set_message_id(request.message_content().message_id());

    // 尝试获取错误消息对象
    if (const auto errorMessage = tlsRegistryManager.globalRegistry.try_get<TipInfoMessage>(GlobalEntity());
        nullptr != errorMessage)
    {
        // 如果存在错误信息，复制到 response 中并清除原始信息
        response.mutable_message_content()->mutable_error_message()->set_id(err);
        errorMessage->Clear();  // 清除原始错误信息
    }
}
