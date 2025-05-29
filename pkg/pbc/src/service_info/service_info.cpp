#include <array>
#include "service_info.h"

std::unordered_set<uint32_t> gClientToServerMessageId;
std::array<RpcService, 1> gMessageInfo;

void InitMessageInfo()
{
}
