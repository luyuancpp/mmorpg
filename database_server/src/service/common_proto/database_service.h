#pragma once
#include "database_service.pb.h"
class DbServiceImpl : public dbservice::DbService{
public:
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
public:
    void Login(::google::protobuf::RpcController* controller,
        const dbservice::LoginRequest* request,
        dbservice::LoginResponse* response,
        ::google::protobuf::Closure* done)override;

    void CreatePlayer(::google::protobuf::RpcController* controller,
        const dbservice::CreatePlayerRequest* request,
        dbservice::CreatePlayerResponse* response,
        ::google::protobuf::Closure* done)override;

    void EnterGame(::google::protobuf::RpcController* controller,
        const dbservice::EnterGameRequest* request,
        dbservice::EnterGameResponse* response,
        ::google::protobuf::Closure* done)override;

};