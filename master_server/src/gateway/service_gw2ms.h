#ifndef MASTER_SERVER_SRC_GATEWAY_SERVICE_GW2MS_H_
#define MASTER_SERVER_SRC_GATEWAY_SERVICE_GW2MS_H_

#include "gw2ms.pb.h"

namespace master
{
    class MasterServer;
}//master

namespace gw2ms
{
    class Gw2msServiceImpl : public gw2ms::Gw2msService
    {
    public:
        Gw2msServiceImpl(master::MasterServer* server)
            :server_(server)
        {

        }

        virtual void GwConnectMaster(::google::protobuf::RpcController* controller,
            const ::gw2ms::ConnectRequest* request,
            ::google::protobuf::Empty* response,
            ::google::protobuf::Closure* done)override;

    private:
        master::MasterServer* server_{ nullptr };
    };
}//namespace gw2ms

#endif//MASTER_SERVER_SRC_GATEWAY_SERVICE_GW2MS_H_
