#include <array>
#include <memory>
#include <google/protobuf/message.h>

#include "gsscene_normal.h"
#include "gsteam_normal.h"
std::array<std::unique_ptr<::google::protobuf::Service>, 2> g_server_nomal_service{
std::unique_ptr<::google::protobuf::Service>(new ServerSceneServiceImpl),
std::unique_ptr<::google::protobuf::Service>(new NormalS2STeamServiceImpl)};
