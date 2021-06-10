#include "login_server.h"

namespace login
{
void LoginServer::Start()
{
    server_.start();
}

void LoginServer::RegisterService(::google::protobuf::Service* service)
{
    server_.registerService(service);
}

}


