#pragma once
#include "database_service.pb.h"
class DbServiceImpl : public dbservice::DbService{
public:
public:
    void Login(::google::protobuf::RpcController* controller,
        const dbservice::DbNodeLoginRequest* request,
        dbservice::DbNodeLoginResponse* response,
        ::google::protobuf::Closure* done)override;

    void CreatePlayer(::google::protobuf::RpcController* controller,
        const dbservice::DbNodeCreatePlayerRequest* request,
        dbservice::DbNodeCreatePlayerResponse* response,
        ::google::protobuf::Closure* done)override;

    void EnterGame(::google::protobuf::RpcController* controller,
        const dbservice::DbNodeEnterGameRequest* request,
        dbservice::DbNodeEnterGameResponse* response,
        ::google::protobuf::Closure* done)override;

};