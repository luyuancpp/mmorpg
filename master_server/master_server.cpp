#include "master_server.h"

namespace master
{
void MasterServer::Start()
{
    server_.start();
}

void MasterServer::RegisterService(::google::protobuf::Service* service)
{
    server_.registerService(service);
}
}//namespace master