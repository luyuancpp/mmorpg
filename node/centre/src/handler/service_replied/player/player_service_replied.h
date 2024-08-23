#pragma once
#include <memory>
#include <unordered_map>

#include "google/protobuf/message.h"
#include "google/protobuf/service.h"

#include "util/game_registry.h"


class PlayerServiceReplied
{
public:
	PlayerServiceReplied(std::unique_ptr<::google::protobuf::Service>&& pb_service)
		: pb_service_(std::move(pb_service))
	{

	}

	virtual void CallMethod(const ::google::protobuf::MethodDescriptor* method,
		entt::entity player,
		const ::google::protobuf::Message* request,
		::google::protobuf::Message* response) = 0;

    ::google::protobuf::Service* service() { return pb_service_.get(); }
private:
    std::unique_ptr<::google::protobuf::Service> pb_service_;
};

void InitPlayerServiceReplied();

extern std::unordered_map<std::string, std::unique_ptr<PlayerServiceReplied>> g_player_service_replied;


