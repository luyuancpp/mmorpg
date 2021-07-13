#include "deploy_server.h"

namespace deploy_server
{
    void DeployServer::Start()
    {
        server_.start();
    }

    void DeployServer::RegisterService(google::protobuf::Service* service)
    {
        server_.registerService(service);
    }
}//namespace deploy_server