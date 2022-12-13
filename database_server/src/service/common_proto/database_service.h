#pragma once
#include "database_service.pb.h"
class DbServiceImpl : public ::DbService{
public:
public:
    void Login(::google::protobuf::RpcController* controller,
        const ::DbNodeLoginRequest* request,
        ::DbNodeLoginResponse* response,
        ::google::protobuf::Closure* done)override;

    void CreatePlayer(::google::protobuf::RpcController* controller,
        const ::DbNodeCreatePlayerRequest* request,
        ::DbNodeCreatePlayerResponse* response,
        ::google::protobuf::Closure* done)override;

    void EnterGame(::google::protobuf::RpcController* controller,
        const ::DbNodeEnterGameRequest* request,
        ::DbNodeEnterGameResponse* response,
        ::google::protobuf::Closure* done)override;

};