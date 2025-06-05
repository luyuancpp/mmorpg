#pragma once
#include "type_define/type_define.h"
#include <memory>
#include <grpcpp/client_context.h>

NodeId GetGateNodeId(Guid session_id);

class  SessionDetails;
using SessionDetailsPtr = std::unique_ptr<SessionDetails>;

SessionDetailsPtr GetSessionDetailsByClientContext(const grpc::ClientContext& context);