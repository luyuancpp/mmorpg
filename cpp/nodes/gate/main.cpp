#include "grpc/third_party/abseil-cpp/absl/log/initialize.h"
#include "gate_node.h"
#include "muduo/base/CrossPlatformAdapterFunction.h"

int main(int argc, char* argv[])
{
	absl::InitializeLog();

    EventLoop loop;
    GateNode node(&loop);
    loop.loop();
    google::protobuf::ShutdownProtobufLibrary();

    return 0;
}