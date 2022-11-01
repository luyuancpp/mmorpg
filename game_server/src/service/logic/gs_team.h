#pragma once
#include "src/pb/pbc/logic_proto/team.pb.h"
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
class NormalS2STeamServiceImpl : public ::NormalS2STeamService{
public:
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
public:
    void EnterScene(::google::protobuf::RpcController* controller,
        const ::TeamTestS2SRequest* request,
        ::TeamTestS2SRespone* response,
        ::google::protobuf::Closure* done)override;

};