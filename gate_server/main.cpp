#include "src/gate_server.h"
#include "src/network/gate_player_list.h"

int main(int argc, char* argv[])
{
	muduo::Logger::setLogLevel(muduo::Logger::ERROR_);
    EventLoop loop;
    GateServer server(&loop);
    server.Init();
    loop.loop();
    google::protobuf::ShutdownProtobufLibrary();
    return 0;
}