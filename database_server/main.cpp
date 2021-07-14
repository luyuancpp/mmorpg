#include "muduo/base/Logging.h"
#include "muduo/net/EventLoop.h"
#include "src/game_rpc/game_rpc_server.h"

#include "src/login/service.h"

#include "database_server.h"

#include "l2db.pb.h"

using namespace muduo;
using namespace muduo::net;
using namespace database;
using namespace common;

int32_t main(int argc, char* argv[])
{
    ConnetionParam query_database_param;
    query_database_param.set_host_name("127.0.0.1");
    query_database_param.set_user_name("root");
    query_database_param.set_pass_word("luyuan616586");
    query_database_param.set_database_name("game");
    query_database_param.set_port(3306);
    EventLoop loop;
    InetAddress listenAddr("127.0.0.1", 2003);
    DatabaseServer server(&loop, listenAddr, query_database_param);
    l2db::LoginServiceImpl impl;
    impl.set_player_mysql_client(server.player_mysql_client());
    impl.set_redis_client(server.redis_client());
    server.RegisterService(&impl);
    server.Start();
    loop.loop();
    return 0;
}

