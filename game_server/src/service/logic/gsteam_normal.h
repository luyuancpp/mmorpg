#pragma once
#include "src/pb/pbc/logic_proto/team_normal.pb.h"
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
class ServerTeamServiceImpl : public normal::ServerTeamService{
public:
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
public:
    void EnterScene(::google::protobuf::RpcController* controller,
        const normal::TeamTestS2SRequest* request,
        normal::TeamTestS2SRespone* response,
        ::google::protobuf::Closure* done)override;

};