#include "game_scene_handler.h"
#include "game_service_handler.h"
std::array<std::unique_ptr<::google::protobuf::Service>, 2> g_server_service{
std::unique_ptr<::google::protobuf::Service>(new GameSceneServiceHandler),
std::unique_ptr<::google::protobuf::Service>(new GameServiceHandler),
};
