#include "muduo/net/EventLoop.h"

#include "src/centre_node.h"
#include "src/comp/player_list.h"

using namespace muduo;
using namespace muduo::net;

int main(int argc, char* argv[])
{
 
    EventLoop loop;
    CentreNode server(&loop);
    server.Init();
    loop.loop();
    google::protobuf::ShutdownProtobufLibrary();
    return 0;
}