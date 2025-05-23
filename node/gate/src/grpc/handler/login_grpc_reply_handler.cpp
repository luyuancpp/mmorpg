#include "muduo/base/Logging.h"

#include "gate_node.h"
#include "grpc/generator/proto/common/login_service_grpc.h"
#include "thread_local/storage_gate.h"

using GrpcLoginStubPtr = std::unique_ptr<LoginService::Stub>;

void InitGrpcLoginServiceResponseHandler() {
    {
        using Function = std::function<void(const std::unique_ptr<AsyncLoginServiceLoginGrpcClientCall>&)>;
        extern Function AsyncLoginServiceLoginHandler;
        AsyncLoginServiceLoginHandler = [](const std::unique_ptr<AsyncLoginServiceLoginGrpcClientCall>& call) {
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
        using Function = std::function<void(const std::unique_ptr<AsyncLoginServiceCreatePlayerGrpcClientCall>&)>;
        extern Function AsyncLoginServiceCreatePlayerHandler;
        AsyncLoginServiceCreatePlayerHandler = [](const std::unique_ptr<AsyncLoginServiceCreatePlayerGrpcClientCall>& call) {
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
        using Function = std::function<void(const std::unique_ptr<AsyncLoginServiceEnterGameGrpcClientCall>&)>;
        extern Function AsyncLoginServiceEnterGameHandler;
        AsyncLoginServiceEnterGameHandler = [](const std::unique_ptr<AsyncLoginServiceEnterGameGrpcClientCall>& call) {
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
        using Function = std::function<void(const std::unique_ptr<AsyncLoginServiceLeaveGameGrpcClientCall>&)>;
        extern Function AsyncLoginServiceLeaveGameHandler;
        AsyncLoginServiceLeaveGameHandler = [](const std::unique_ptr<AsyncLoginServiceLeaveGameGrpcClientCall>& call) {
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
