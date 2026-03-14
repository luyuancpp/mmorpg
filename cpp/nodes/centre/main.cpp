#include "muduo/net/EventLoop.h"

#include "node/system/node/simple_node.h"
#include "handler/rpc/centre_service_handler.h"
#include "handler/event/event_handler.h"
#include "redis/system/redis.h"
#include "grpc/third_party/abseil-cpp/absl/log/initialize.h"
#include "proto/common/base/node.pb.h"

using namespace muduo;
using namespace muduo::net;

namespace {
void startCentreNode(EventLoop& loop)
{
    SimpleNode<CentreHandler> node(&loop, "logs/centre", CentreNodeService);
    EventHandler::Register();
    tlsRedisSystem.Initialize();
    loop.loop();
}
}

int main(int argc, char* argv[])
{
    absl::InitializeLog();

    EventLoop loop;
    startCentreNode(loop);
    google::protobuf::ShutdownProtobufLibrary();
    return 0;
}