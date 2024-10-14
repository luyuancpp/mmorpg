#include "grpc/third_party/abseil-cpp/absl/log/initialize.h"
#include "src/gate_node.h"
#include "muduo/base/CrossPlatformAdapterFunction.h"

int main(int argc, char* argv[])
{
	absl::InitializeLog();

    EventLoop loop;
    GateNode server(&loop);
    server.Init();
    loop.loop();
    google::protobuf::ShutdownProtobufLibrary();

    return 0;
}