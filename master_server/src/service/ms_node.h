#ifndef MASTER_SERVER_SRC_SERVICE_MS_NODE_H_
#define MASTER_SERVER_SRC_SERVICE_MS_NODE_H_
#include "ms_node.pb.h"
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
namespace msservice{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
class MasterNodeServiceImpl : public MasterNodeService{
public:
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
public:
    void StartGS(::google::protobuf::RpcController* controller,
        const msservice::StartGSRequest* request,
        msservice::StartGSResponse* response,
        ::google::protobuf::Closure* done)override;

    void OnGwConnect(::google::protobuf::RpcController* controller,
        const msservice::ConnectRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

    void OnGwPlayerDisconnect(::google::protobuf::RpcController* controller,
        const msservice::PlayerDisconnectRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

    void OnGwLeaveGame(::google::protobuf::RpcController* controller,
        const msservice::LeaveGameRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

    void OnGwPlayerService(::google::protobuf::RpcController* controller,
        const msservice::ClientMessageRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

    void OnGwDisconnect(::google::protobuf::RpcController* controller,
        const msservice::DisconnectRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

};
}// namespace msservice
#endif//MASTER_SERVER_SRC_SERVICE_MS_NODE_H_
