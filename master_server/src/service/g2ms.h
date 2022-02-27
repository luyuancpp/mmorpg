#ifndef MASTER_SERVER_SRC_GAME_SERVICE_G2MS_H_
#define MASTER_SERVER_SRC_GAME_SERVICE_G2MS_H_

#include "g2ms.pb.h"
///<<< BEGIN WRITING YOUR CODE
namespace master
{
    class MasterServer;
}//master
///<<< END WRITING YOUR CODE
namespace g2ms
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
    class G2msServiceImpl : public g2ms::G2msService
    {
    public:
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
    public:
        virtual void StartGS(::google::protobuf::RpcController* controller,
            const ::g2ms::StartGSRequest* request,
            ::g2ms::StartGSResponse* response,
            ::google::protobuf::Closure* done)override;
    };

}//namespace g2ms

#endif//MASTER_SERVER_SRC_GAME_SERVICE_G2MS_H_
