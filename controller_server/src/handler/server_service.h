#pragma once
#include <array>
#include <memory>
#include <google/protobuf/message.h>

extern std::array<std::unique_ptr<::google::protobuf::Service>, 2> g_server_service;
