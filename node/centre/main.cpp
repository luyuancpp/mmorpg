#include "muduo/net/EventLoop.h"

#include "grpc/third_party/abseil-cpp/absl/log/initialize.h"
#include "src/centre_node.h"

using namespace muduo;
using namespace muduo::net;

int main(int argc, char* argv[])
{
    absl::InitializeLog();

    EventLoop loop;
    CentreNode server(&loop);
    server.Initialize();
    loop.loop();
    google::protobuf::ShutdownProtobufLibrary();
    return 0;
}