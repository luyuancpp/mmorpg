#pragma once

#include <thread_context/ecs_context.h>

// Send error response to client
template <typename Request, typename Response>
void SendErrorToClient(const Request& request, Response& response, uint32_t err) {
    // Preserve header/session consistency if Request has a header
    if constexpr (std::is_member_function_pointer_v<decltype(&Request::header)>) {
        response.mutable_header()->set_session_id(request.header().session_id());
    }

    response.mutable_message_content()->set_message_id(request.message_content().message_id());

    auto& tip = tlsEcs.globalRegistry.get_or_emplace<TipInfoMessage>(tlsEcs.GlobalEntity());
    response.mutable_message_content()->mutable_error_message()->CopyFrom(tip);
    response.mutable_message_content()->mutable_error_message()->set_id(err);
    // Decide whether to clear based on semantics (avoid unconditionally clearing global state)
    // tip->Clear();
}

