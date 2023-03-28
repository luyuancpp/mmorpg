#include <unordered_set>

#include "service_method_id.h"
#include "gameservice_service_method_id.h"
#include "loginservice_service_method_id.h"
#include "clientplayercommonservice_service_method_id.h"
#include "serverplayerloginservice_service_method_id.h"
#include "lobbyservice_service_method_id.h"
#include "serversceneservice_service_method_id.h"
#include "clientplayersceneservice_service_method_id.h"
#include "serverplayersceneservice_service_method_id.h"
#include "s2steamservice_service_method_id.h"
#include "clientplayerteamservice_service_method_id.h"
#include "serverplayerteamservice_service_method_id.h"

std::unordered_set<uint32_t> g_player_service_method_id{
ClientPlayerSceneService_Id_EnterSceneC2S
};
