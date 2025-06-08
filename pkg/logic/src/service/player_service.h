#pragma once
#include <memory>
#include <unordered_map>
#include <entt/src/entt/entity/entity.hpp>
#include "google/protobuf/service.h"

#include "thread_local/storage.h"

class PlayerService
{
public:
	PlayerService(std::unique_ptr<::google::protobuf::Service>&& pb_service)
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

void InitPlayerService();

extern std::unordered_map<std::string, std::unique_ptr<PlayerService>> gPlayerService;


