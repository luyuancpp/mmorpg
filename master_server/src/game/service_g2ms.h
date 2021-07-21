#ifndef MASTER_SERVER_SRC_GAME_SERVICE_G2MS_H_
#define MASTER_SERVER_SRC_GAME_SERVICE_G2MS_H_

#include "g2ms.pb.h"

namespace master
{
    class MasterServer;
}//master

namespace g2ms
{
    class G2msServiceImpl : public g2ms::G2msService
    {
    public:
        G2msServiceImpl(master::MasterServer* server)
            :server_(server)
        {

        }

        virtual void StartLogicServer(::google::protobuf::RpcController* controller,
            const ::g2ms::StartLogicServerRequest* request,
            ::google::protobuf::Empty* response,
            ::google::protobuf::Closure* done)override;
    private:
        master::MasterServer* server_{ nullptr };
    };

}//namespace g2ms

#endif//MASTER_SERVER_SRC_GAME_SERVICE_G2MS_H_
