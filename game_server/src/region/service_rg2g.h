#ifndef GAME_SERVER_SRC_MASTER_SERVICE_MS2G_H_
#define GAME_SERVER_SRC_MASTER_SERVICE_MS2G_H_

#include "rg2g.pb.h"

namespace rg2g
{
class Rg2gServiceImpl : public Rg2gService
{
public:
    virtual void EnterRoom(::google::protobuf::RpcController* controller,
        const ::google::protobuf::Empty* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

private:

};
}//namespace rg2g

#endif//GAME_SERVER_SRC_MASTER_SERVICE_MS2G_H_
