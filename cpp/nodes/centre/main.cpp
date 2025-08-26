#include "muduo/net/EventLoop.h"

#include "grpc/third_party/abseil-cpp/absl/log/initialize.h"
#include "centre_node.h"

using namespace muduo;
using namespace muduo::net;

int main(int argc, char* argv[])
{
    absl::InitializeLog();

    EventLoop loop;
    CentreNode node(&loop);
    loop.loop();
    google::protobuf::ShutdownProtobufLibrary();
    return 0;
}