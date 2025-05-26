#include "muduo/base/Logging.h"

#include "gate_node.h"
#include "grpc/generator/proto/common/login_service_grpc.h"
#include "thread_local/storage_gate.h"

using GrpcLoginStubPtr = std::unique_ptr<loginpb::LoginService::Stub>;

void InitGrpcLoginServiceResponseHandler() {
    {
        loginpb::AsyncloginpbLoginServiceLoginHandler = [](const std::unique_ptr<loginpb::AsyncloginpbLoginServiceLoginGrpcClientCall>& call) {
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
        loginpb::AsyncloginpbLoginServiceCreatePlayerHandler = [](const std::unique_ptr<loginpb::AsyncloginpbLoginServiceCreatePlayerGrpcClientCall>& call) {
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
        loginpb::AsyncloginpbLoginServiceEnterGameHandler = [](const std::unique_ptr<loginpb::AsyncloginpbLoginServiceEnterGameGrpcClientCall>& call) {
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
        loginpb::AsyncloginpbLoginServiceLeaveGameHandler = [](const std::unique_ptr<loginpb::AsyncloginpbLoginServiceLeaveGameGrpcClientCall>& call) {
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
