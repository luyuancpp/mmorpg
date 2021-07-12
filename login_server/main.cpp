#include "login_server.h"

#include "src/database/rpcclient/database_rpcclient.h"
#include "src/gateway/service.h"
#include "src/master/rpcclient/master_rpcclient.h"

using namespace login;

int main(int argc, char* argv[])
{
    EventLoop loop;
    InetAddress listen_addr("127.0.0.1", 2001);
    InetAddress database_addr("127.0.0.1", 2003);
    InetAddress master_addr("127.0.0.1", 2004);

    GetSingletonLoginStub() = std::make_unique<common::RpcClient<l2db::LoginService_Stub>>(&loop, database_addr);
    GetSingletonLoginStub()->connect();
    login::MasterRpcClient::GetSingleton().Connect(&loop, master_addr);

    gw2l::LoginServiceImpl impl;
    LoginServer server(&loop, listen_addr);
    server.RegisterService(&impl);
    impl.set_redis_client(server.redis_client());
    server.Start();
    loop.loop();
    return 0;
}