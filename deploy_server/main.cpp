#include "muduo/base/Logging.h"
#include "muduo/net/EventLoop.h"

#include "deploy_server.h"

using namespace muduo::net;

int32_t main(int argc, char* argv[])
{
    /*ConnectionParameters query_database_param{ "127.0.0.1", "root" , "luyuan616586", "game" , 3306 };
    EventLoop loop;
    InetAddress listenAddr("127.0.0.1", 2003);
    DeployServer server(&loop, listenAddr, query_database_param);
    l2db::LoginServiceImpl impl;
    impl.set_player_mysql_client(server.player_mysql_client());
    impl.set_redis_client(server.redis_client());
    server.RegisterService(&impl);
    server.Start();
    loop.loop();*/
    return 0;
}

