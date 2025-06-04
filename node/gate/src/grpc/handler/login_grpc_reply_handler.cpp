#include "muduo/base/Logging.h"

#include "gate_node.h"
#include "grpc/generator/proto/login/login_service_grpc.h"
#include "thread_local/storage_gate.h"

using GrpcLoginStubPtr = std::unique_ptr<loginpb::ClientPlayerLogin::Stub>;

void InitGrpcClientPlayerLoginResponseHandler() {
    {
        loginpb::AsyncClientPlayerLoginLoginHandler = [](const std::unique_ptr<loginpb::AsyncClientPlayerLoginLoginGrpcClientCall>& call) {
            SessionDetails sessionDetails;
            call->context.GetServerTrailingMetadata();
            auto it = tls_gate.sessions().find(call->reply.session_info().session_id());
            if (it == tls_gate.sessions().end())
            {
                LOG_DEBUG << "conn id not found  session id " << "," << call->reply.session_info().session_id();
                return;
            }
            gGateNode->SendMessageToClient(it->second.conn, call->reply.client_msg_body());
        };
    }

    {
        loginpb::AsyncClientPlayerLoginCreatePlayerHandler = [](const std::unique_ptr<loginpb::AsyncClientPlayerLoginCreatePlayerGrpcClientCall>& call) {
            auto it = tls_gate.sessions().find(call->reply.session_info().session_id());
            if (it == tls_gate.sessions().end())
            {
                LOG_DEBUG << "conn id not found  session id " << "," << call->reply.session_info().session_id();
                return;
            }
            gGateNode->SendMessageToClient(it->second.conn, call->reply.client_msg_body());
        };
    }

    {
        loginpb::AsyncClientPlayerLoginEnterGameHandler = [](const std::unique_ptr<loginpb::AsyncClientPlayerLoginEnterGameGrpcClientCall>& call) {
            auto it = tls_gate.sessions().find(call->reply.session_info().session_id());
            if (it == tls_gate.sessions().end())
            {
                LOG_DEBUG << "conn id not found  session id " << "," << call->reply.session_info().session_id();
                return;
            }
            gGateNode->SendMessageToClient(it->second.conn, call->reply.client_msg_body());
        };
    }


    {
        loginpb::AsyncClientPlayerLoginLeaveGameHandler = [](const std::unique_ptr<loginpb::AsyncClientPlayerLoginLeaveGameGrpcClientCall>& call) {
          /*  auto it = tls_gate.sessions().find(call->reply.session_info().session_id());
            if (it == tls_gate.sessions().end())
            {
                LOG_DEBUG << "conn id not found  session id " << "," << call->reply.session_info().session_id();
                return;
            }
            g_gate_node->SendMessageToClient(it->second.conn, call->reply.client_msg_body());*/
        };
    }

  
}
