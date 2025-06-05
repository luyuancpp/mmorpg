#include "muduo/base/Logging.h"

#include "gate_node.h"
#include "grpc/generator/proto/login/login_service_grpc.h"
#include "thread_local/storage_gate.h"
#include <game_common_logic/system/session_system.h>

using GrpcLoginStubPtr = std::unique_ptr<loginpb::ClientPlayerLogin::Stub>;

void InitGrpcClientPlayerLoginResponseHandler() {

	auto sendGrpcResponseToClientSession = [](const ClientContext& context, const ::google::protobuf::Message& reply) {
		auto sessionDetails = GetSessionDetailsByClientContext(context);
		if (nullptr == sessionDetails) {
			LOG_ERROR << "Session details not found in context for session id";
			return;
		}
		auto it = tls_gate.sessions().find(sessionDetails->session_id());
		if (it == tls_gate.sessions().end())
		{
			LOG_DEBUG << "conn id not found  session id " << "," << sessionDetails->session_id();
			return;
		}
		gGateNode->SendMessageToClient(it->second.conn, reply);
		};

	loginpb::AsyncClientPlayerLoginLoginHandler = sendGrpcResponseToClientSession;
	loginpb::AsyncClientPlayerLoginCreatePlayerHandler = sendGrpcResponseToClientSession;
	loginpb::AsyncClientPlayerLoginEnterGameHandler = sendGrpcResponseToClientSession;
	loginpb::AsyncClientPlayerLoginLeaveGameHandler = sendGrpcResponseToClientSession;
}
