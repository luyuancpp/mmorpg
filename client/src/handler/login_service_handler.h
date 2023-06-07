#pragma once
#include <sol/sol.hpp>
#include "src/game_logic/thread_local/thread_local_storage_lua.h"
#include "common_proto/login_service.pb.h"

class LoginServiceHandler : public ::LoginService
{
public:
	void CallMethod(const ::google::protobuf::MethodDescriptor* method,
	::google::protobuf::RpcController* controller,
	const ::google::protobuf::Message* request,
	::google::protobuf::Message* response,
	::google::protobuf::Closure* done)override
	{
		 switch(method->index())
		{
			case 0:
			{
				tls_lua_state["Login"](
				::google::protobuf::internal::DownCast<const ::LoginRequest*>( request),
				::google::protobuf::internal::DownCast<::LoginResponse*>(response));
			}
			break;
			case 1:
			{
				tls_lua_state["CreatPlayer"](
				::google::protobuf::internal::DownCast<const ::CreatePlayerC2lRequest*>( request),
				::google::protobuf::internal::DownCast<::LoginNodeCreatePlayerResponse*>(response));
			}
			break;
			case 2:
			{
				tls_lua_state["EnterGame"](
				::google::protobuf::internal::DownCast<const ::EnterGameC2LRequest*>( request),
				::google::protobuf::internal::DownCast<::LoginNodeEnterGameResponse*>(response));
			}
			break;
			case 3:
			{
				tls_lua_state["LeaveGame"](
				::google::protobuf::internal::DownCast<const ::LeaveGameC2LRequest*>( request),
				::google::protobuf::internal::DownCast<::google::protobuf::Empty*>(response));
			}
			break;
			case 4:
			{
				tls_lua_state["Disconnect"](
				::google::protobuf::internal::DownCast<const ::LoginNodeDisconnectRequest*>( request),
				::google::protobuf::internal::DownCast<::google::protobuf::Empty*>(response));
			}
			break;
			case 5:
			{
				tls_lua_state["RouteNodeStringMsg"](
				::google::protobuf::internal::DownCast<const ::RouteMsgStringRequest*>( request),
				::google::protobuf::internal::DownCast<::RouteMsgStringResponse*>(response));
			}
			break;
			case 6:
			{
				tls_lua_state["RoutePlayerStringMsg"](
				::google::protobuf::internal::DownCast<const ::RoutePlayerMsgStringRequest*>( request),
				::google::protobuf::internal::DownCast<::RoutePlayerMsgStringResponse*>(response));
			}
			break;
			case 7:
			{
				tls_lua_state["GateConnect"](
				::google::protobuf::internal::DownCast<const ::GateConnectRequest*>( request),
				::google::protobuf::internal::DownCast<::google::protobuf::Empty*>(response));
			}
			break;
			default:
				GOOGLE_LOG(FATAL) << "Bad method index; this should never happen."
;			break;
		};
	};
};