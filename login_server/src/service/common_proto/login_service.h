#pragma once
#include "login_service.pb.h"
class LoginServiceImpl : public loginservice::LoginService{
public:
public:
    void Login(::google::protobuf::RpcController* controller,
        const loginservice::LoginNodeLoginRequest* request,
        loginservice::LoginNodeLoginResponse* response,
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

    void RouteNodeStringMsg(::google::protobuf::RpcController* controller,
        const ::RouteMsgStringRequest* request,
        ::RouteMsgStringResponse* response,
        ::google::protobuf::Closure* done)override;

    void RoutePlayerStringMsg(::google::protobuf::RpcController* controller,
        const ::RoutePlayerMsgStringRequest* request,
        ::RoutePlayerMsgStringResponse* response,
        ::google::protobuf::Closure* done)override;

};