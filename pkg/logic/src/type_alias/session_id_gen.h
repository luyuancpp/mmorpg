#pragma once
#include "type_define/type_define.h"
#include "util/snow_flake.h"

using SessionIdGen = NodeSequence<uint64_t, 32>;

