#pragma once
#include "common_proto/database_service.pb.h"
class AccountDBServiceHandler : public ::AccountDBService
{
public:
	void Load2Redis(::google::protobuf::RpcController* controller,
		const ::LoadAccountRequest* request,
		::LoadAccountResponse* response,
		 ::google::protobuf::Closure* done)override;

	void Save2Redis(::google::protobuf::RpcController* controller,
		const ::SaveAccountRequest* request,
		::SaveAccountResponse* response,
		 ::google::protobuf::Closure* done)override;

};

