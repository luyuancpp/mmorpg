#pragma once
#include <memory>
#include <unordered_map>

#include "google/protobuf/message.h"
#include "google/protobuf/service.h"

#include "src/game_logic/thread_local/thread_local_storage.h"


class PlayerService
{
public:
    PlayerService(::google::protobuf::Service* pb_service)
        : pb_service_(pb_service) {}

    virtual void CallMethod(const ::google::protobuf::MethodDescriptor* method,
        entt::entity player,
        const ::google::protobuf::Message* request,
        ::google::protobuf::Message* response) = 0;

    ::google::protobuf::Service* service() { return pb_service_.get(); }
private:
    std::unique_ptr<::google::protobuf::Service> pb_service_;
};

void InitPlayerServcie();

extern std::unordered_map<std::string, std::unique_ptr<PlayerService>> g_player_services;
extern std::unordered_set<std::string> g_open_player_services;//开放给玩家的服务


