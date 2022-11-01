#include <array>
#include <memory>
#include <google/protobuf/message.h>

#include "controller_scene.h"
#include "controller_team.h"
std::array<std::unique_ptr<::google::protobuf::Service>, 2> g_server_nomal_service{
std::unique_ptr<::google::protobuf::Service>(new ServerSceneServiceImpl),
std::unique_ptr<::google::protobuf::Service>(new NormalS2STeamServiceImpl)};
