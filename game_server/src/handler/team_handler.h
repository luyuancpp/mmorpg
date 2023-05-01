#pragma once
#include "team.pb.h"
class TeamServiceHandler : public ::TeamService
{
public:
	void EnterScene(::google::protobuf::RpcController* controller,
		const ::TeamTestS2SRequest* request,
		::TeamTestS2SRespone* response,
		 ::google::protobuf::Closure* done)override;

};

