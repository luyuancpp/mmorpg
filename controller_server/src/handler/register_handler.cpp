#include "controller_scene_handler.h"
#include "controller_service_handler.h"
std::array<std::unique_ptr<::google::protobuf::Service>, 2> g_server_service{
std::unique_ptr<::google::protobuf::Service>(new ControllerSceneServiceHandler),
std::unique_ptr<::google::protobuf::Service>(new ControllerServiceHandler),
};
