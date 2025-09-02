#include <memory>
#include <string>
#include <unordered_map>
#include "rpc/player_service_interface.h"

std::unordered_map<std::string, std::unique_ptr<PlayerService>> gPlayerService;

void InitPlayerService()
{
}
