#ifndef MASTER_SERVER_SRC_SERVICE_GS2MS_H_
#define MASTER_SERVER_SRC_SERVICE_GS2MS_H_
#include "gs2ms.pb.h"
///<<< BEGIN WRITING YOUR CODE
namespace master
{
    class MasterServer;
}//master
///<<< END WRITING YOUR CODE
namespace gs2ms{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
class G2msServiceImpl : public G2msService{
public:
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
public:
    void StartGS(::google::protobuf::RpcController* controller,
        const gs2ms::StartGSRequest* request,
        gs2ms::StartGSResponse* response,
        ::google::protobuf::Closure* done)override;

};
}// namespace gs2ms
#endif//MASTER_SERVER_SRC_SERVICE_GS2MS_H_
