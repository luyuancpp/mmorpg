#pragma once
#include "team.pb.h"
class S2STeamServiceHandler : public ::S2STeamService
{
public:
	void EnterScene(::google::protobuf::RpcController* controller,
		const ::TeamTestS2SRequest* request,
		::TeamTestS2SRespone* response,
		 ::google::protobuf::Closure* done)override;

};

