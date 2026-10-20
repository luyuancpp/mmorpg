#include "node/system/node/grpc_runtime_probe.h"

// 只为读初始化状态才 include 官方 gRPC 头;这里不得加任何会初始化运行时的调用。契约见头文件。
#include <grpc/grpc.h>

namespace node {
namespace observability {

bool IsGrpcRuntimeInitialized()
{
    return grpc_is_initialized() != 0;
}

} // namespace observability
} // namespace node
