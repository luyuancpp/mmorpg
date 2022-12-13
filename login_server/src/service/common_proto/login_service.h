#pragma once
#include "login_service.pb.h"
class LoginServiceImpl : public ::LoginService{
public:
public:
    void Login(::google::protobuf::RpcController* controller,
        const ::LoginNodeLoginRequest* request,
        ::LoginNodeLoginResponse* response,
        ::google::protobuf::Closure* done)override;

    void CreatPlayer(::google::protobuf::RpcController* controller,
        const ::LoginNodeCreatePlayerRequest* request,
        ::LoginNodeCreatePlayerResponse* response,
        ::google::protobuf::Closure* done)override;

    void EnterGame(::google::protobuf::RpcController* controller,
        const ::LoginNodeEnterGameRequest* request,
        ::LoginNodeEnterGameResponse* response,
        ::google::protobuf::Closure* done)override;

    void LeaveGame(::google::protobuf::RpcController* controller,
        const ::LoginNodeLeaveGameRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

    void Disconnect(::google::protobuf::RpcController* controller,
        const ::LoginNodeDisconnectRequest* request,
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