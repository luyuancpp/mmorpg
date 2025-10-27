#pragma once

#include <threading/registry_manager.h>

entt::entity GlobalEntity();

// 模板函数：发送错误信息给客户端
template <typename Request, typename Response>
void SendErrorToClient(const Request& request, Response& response, uint32_t err) {
    // 保证 header/session 一致（如果 Request 有 header）
    if constexpr (std::is_member_function_pointer_v<decltype(&Request::header)>) {
        response.mutable_header()->set_session_id(request.header().session_id());
    }

    response.mutable_message_content()->set_message_id(request.message_content().message_id());

    if (const auto* tip = tlsRegistryManager.globalRegistry.try_get<TipInfoMessage>(GlobalEntity()); tip != nullptr) {
        response.mutable_message_content()->mutable_error_message()->CopyFrom(*tip);
        response.mutable_message_content()->mutable_error_message()->set_id(err);
        // 根据语义决定是否 clear（不建议无条件清全局)
        // tip->Clear();
    } else {
        response.mutable_message_content()->mutable_error_message()->set_id(err);
    }
}
