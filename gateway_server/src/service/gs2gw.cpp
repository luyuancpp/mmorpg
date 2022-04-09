#include "gs2gw.h"
#include "src/network/rpc_closure.h"
///<<< BEGIN WRITING YOUR CODE
#include "muduo/base/Logging.h"

#include "src/gate_player/gate_player_list.h"
#include "src/gateway_server.h"
using namespace gateway;
///<<< END WRITING YOUR CODE

using namespace common;
namespace gs2gw{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

///<<<rpc begin
void Gs2GwServiceImpl::PlayerService(::google::protobuf::RpcController* controller,
    const gs2gw::PlayerMessageRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE PlayerService
	auto conn_id = request->ex().conn_id();
	auto it = g_client_sessions_->find(conn_id);
	if (it == g_client_sessions_->end())
	{
		LOG_ERROR << "connid not found  conn id " << conn_id;
		return;
	}
	g_gateway_server->Send2Client(it->second.conn_, request->msg());
///<<< END WRITING YOUR CODE PlayerService
}

///<<<rpc end
}// namespace gs2gw
