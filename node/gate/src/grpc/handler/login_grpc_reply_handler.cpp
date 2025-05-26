#include "muduo/base/Logging.h"

#include "gate_node.h"
#include "grpc/generator/proto/common/login_service_grpc.h"
#include "thread_local/storage_gate.h"

using GrpcLoginStubPtr = std::unique_ptr<loginpb::LoginService::Stub>;

void InitGrpcLoginServiceResponseHandler() {
    {
        loginpb::AsyncLoginServiceLoginHandler = [](const std::unique_ptr<loginpb::AsyncLoginServiceLoginGrpcClientCall>& call) {
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
        loginpb::AsyncLoginServiceCreatePlayerHandler = [](const std::unique_ptr<loginpb::AsyncLoginServiceCreatePlayerGrpcClientCall>& call) {
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
        loginpb::AsyncLoginServiceEnterGameHandler = [](const std::unique_ptr<loginpb::AsyncLoginServiceEnterGameGrpcClientCall>& call) {
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
        loginpb::AsyncLoginServiceLeaveGameHandler = [](const std::unique_ptr<loginpb::AsyncLoginServiceLeaveGameGrpcClientCall>& call) {
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
