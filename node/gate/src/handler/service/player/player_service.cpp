#include <memory>
#include <string>
#include <unordered_map>
#include "service/player_service.h"

std::unordered_map<std::string, std::unique_ptr<PlayerService>> gPlayerService;

void InitPlayerService()
{
}
