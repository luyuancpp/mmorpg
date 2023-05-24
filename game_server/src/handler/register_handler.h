#pragma once
#include <array>
#include <memory>
#include <google/protobuf/service.h>

extern std::unordered_map<std::string, std::unique_ptr<::google::protobuf::Service>> g_server_service;
