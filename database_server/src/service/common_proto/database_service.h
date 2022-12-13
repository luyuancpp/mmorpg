#pragma once
#include "database_service.pb.h"
class DbServiceImpl : public ::DbService{
public:
public:
    void Login(::google::protobuf::RpcController* controller,
        const ::DatabaseNodeLoginRequest* request,
        ::DatabaseNodeLoginResponse* response,
        ::google::protobuf::Closure* done)override;

    void CreatePlayer(::google::protobuf::RpcController* controller,
        const ::DatabaseNodeCreatePlayerRequest* request,
        ::DatabaseNodeCreatePlayerResponse* response,
        ::google::protobuf::Closure* done)override;

    void EnterGame(::google::protobuf::RpcController* controller,
        const ::DatabaseNodeEnterGameRequest* request,
        ::DatabaseNodeEnterGameResponse* response,
        ::google::protobuf::Closure* done)override;

};