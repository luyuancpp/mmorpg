#pragma once
#include "type_define/type_define.h"
#include "util/node_id_generator.h"

using SessionIdGenerator = NodeIdGenerator<uint64_t, 32>;

