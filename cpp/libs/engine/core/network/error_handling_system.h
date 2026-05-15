#pragma once

#include <sstream>

#include <thread_context/ecs_context.h>

#include "core/utils/debug/stacktrace_system.h"
#include "error_reporter/error_reporter.h"

// Send error response to client.
//
// Side effect (todo.md #70 + #125): when err != 0, also log the full
// triage context — error code, request proto type, ShortDebugString of the
// request payload, and a stack trace. This is the "rejected-logic must
// surface" rule from todo #97: any rejection path that returns an error
// to the client must leave enough breadcrumbs for an on-call to triage
// without going back to the client to ask "what did you send?".
//
// Side effect (todo.md #250 slice A): also push the rejection into the
// process-wide error_reporter buffer so cross-node aggregation can pick
// it up. Logging gives one node's view; the reporter gives the fleet's.
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
        const std::string protoName = descriptor ? descriptor->full_name() : std::string("<unknown>");
        const auto messageId = request.message_content().message_id();

        LOG_ERROR << "[SendErrorToClient] rejecting RPC: error_code=" << err
                  << " proto=" << protoName
                  << " message_id=" << messageId
                  << " request={ " << request.ShortDebugString() << " }";
        LOG_ERROR << GetCurrentStackTraceAsString(kMaxEntries);

        // todo.md #250 slice A — record into process-wide buffer for
        // periodic shipping (slice C).
        std::ostringstream msg;
        msg << "proto=" << protoName << " message_id=" << messageId;
        error_reporter::Record(err, "rpc_reject", msg.str());
    }
    // Decide whether to clear based on semantics (avoid unconditionally clearing global state)
    // tip->Clear();
}

