#ifndef MASTER_SERVER_SRC_SERVICE_GW2MS_H_
#define MASTER_SERVER_SRC_SERVICE_GW2MS_H_
#include "ms_node.pb.h"
///<<< BEGIN WRITING YOUR CODE
namespace master
{
    class MasterServer;
}//master
///<<< END WRITING YOUR CODE
namespace gw2ms{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
class Gw2msServiceImpl : public Gw2msService{
public:
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
public:
    void GwConnectMaster(::google::protobuf::RpcController* controller,
        const gw2ms::ConnectRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

    void PlayerDisconnect(::google::protobuf::RpcController* controller,
        const gw2ms::PlayerDisconnectRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

    void LeaveGame(::google::protobuf::RpcController* controller,
        const gw2ms::LeaveGameRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

    void PlayerService(::google::protobuf::RpcController* controller,
        const gw2ms::ClientMessageRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

    void Disconnect(::google::protobuf::RpcController* controller,
        const gw2ms::DisconnectRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

};
}// namespace gw2ms
#endif//MASTER_SERVER_SRC_SERVICE_GW2MS_H_
