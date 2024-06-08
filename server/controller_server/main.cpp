#include "muduo/net/EventLoop.h"

#include "src/centre_server.h"
#include "src/comp/player_list.h"

using namespace muduo;
using namespace muduo::net;

int main(int argc, char* argv[])
{
 
    EventLoop loop;
    CentreServer server(&loop);
    server.Init();
    loop.loop();
    google::protobuf::ShutdownProtobufLibrary();
    return 0;
}