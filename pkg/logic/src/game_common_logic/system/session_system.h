#pragma once
#include "type_define/type_define.h"
#include <grpcpp/client_context.h>

NodeId GetGateNodeId(Guid session_id);

class  SessionDetails;

SessionDetails* GetSessionDetailsByClientContext(const grpc::ClientContext& context);