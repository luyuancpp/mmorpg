#include "muduo/net/EventLoop.h"

#include "src/scene_node.h"
#include "grpc/third_party/abseil-cpp/absl/log/initialize.h"

using namespace muduo;
using namespace muduo::net;

int main(int argc, char* argv[])
{
    absl::InitializeLog();
    
    EventLoop loop;
    SceneNode node(&loop);
    loop.loop();
    google::protobuf::ShutdownProtobufLibrary();
    return 0;
}