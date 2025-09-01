#pragma once
#include "engine/core/type_define/type_define.h"
#include <memory>
#include <grpcpp/client_context.h>


class  SessionDetails;
using SessionDetailsPtr = std::unique_ptr<SessionDetails>;

SessionDetailsPtr GetSessionDetailsByClientContext(const grpc::ClientContext& context);