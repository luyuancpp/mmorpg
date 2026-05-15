#pragma once

#include <thread_context/ecs_context.h>

#include "core/utils/debug/stacktrace_system.h"

// Send error response to client.
//
// Side effect (todo.md #70 + #125): when err != 0, also log the full
// triage context — error code, request proto type, ShortDebugString of the
// request payload, and a stack trace. This is the "rejected-logic must
// surface" rule from todo #97: any rejection path that returns an error
// to the client must leave enough breadcrumbs for an on-call to triage
// without going back to the client to ask "what did you send?".
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

    if (err != 0) {
        const auto* descriptor = request.GetDescriptor();
        LOG_ERROR << "[SendErrorToClient] rejecting RPC: error_code=" << err
                  << " proto=" << (descriptor ? descriptor->full_name() : std::string("<unknown>"))
                  << " message_id=" << request.message_content().message_id()
                  << " request={ " << request.ShortDebugString() << " }";
        LOG_ERROR << GetCurrentStackTraceAsString(kMaxEntries);
    }
    // Decide whether to clear based on semantics (avoid unconditionally clearing global state)
    // tip->Clear();
}

