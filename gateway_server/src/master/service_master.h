#ifndef GATEWAY_SERVER_MASTER_SERVICE_SERVICE_MASTER_H_
#define GATEWAY_SERVER_MASTER_SERVICE_SERVICE_MASTER_H_

#include "ms2gw.pb.h"

namespace ms2gw
{
    class Ms2gwServiceImpl : public Ms2gwService
    {
    public:
        virtual void StartLogicServer(::google::protobuf::RpcController* controller,
            const ::ms2gw::StartLogicServerRequest* request,
            google::protobuf::Empty* response,
            ::google::protobuf::Closure* done)override;
    private:

    };
}//namespace gw2ms

#endif//GATEWAY_SERVER_MASTER_SERVICE_SERVICE_MASTER_H_
