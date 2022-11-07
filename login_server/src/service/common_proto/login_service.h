#pragma once
#include "login_service.pb.h"
class LoginServiceImpl : public loginservice::LoginService{
public:
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
public:
    void Login(::google::protobuf::RpcController* controller,
        const loginservice::LoginRequest* request,
        loginservice::LoginResponse* response,
        ::google::protobuf::Closure* done)override;

    void CreatPlayer(::google::protobuf::RpcController* controller,
        const loginservice::CreatePlayerRequest* request,
        loginservice::CreatePlayerResponse* response,
        ::google::protobuf::Closure* done)override;

    void EnterGame(::google::protobuf::RpcController* controller,
        const loginservice::EnterGameRequest* request,
        loginservice::EnterGameResponse* response,
        ::google::protobuf::Closure* done)override;

    void LeaveGame(::google::protobuf::RpcController* controller,
        const loginservice::LeaveGameRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

    void Disconnect(::google::protobuf::RpcController* controller,
        const loginservice::DisconnectRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

};