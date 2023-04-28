#include <array>
#include <memory>
#include <google/protobuf/message.h>

#include "scene_handler.h"
#include "team_handler.h"
std::array<std::unique_ptr<::google::protobuf::Service>, 2> g_server_service{
std::unique_ptr<::google::protobuf::Service>(new ServerSceneServiceHandler),
std::unique_ptr<::google::protobuf::Service>(new S2STeamServiceHandler)};
