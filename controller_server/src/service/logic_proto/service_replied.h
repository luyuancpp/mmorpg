#pragma once

#include <memory>

#include "src/network/rpc_closure.h"

#include "src/pb/pbc/common_proto/common.pb.h"


using GsCallPlayerRpc = std::shared_ptr<NormalClosure<NodeServiceMessageRequest, NodeServiceMessageResponse>>;
void CallPlayerGsReplied(GsCallPlayerRpc rpc);

