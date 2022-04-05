#ifndef GAME_SERVER_SRC_SERVICE_PLAYER_SERVICE_H_
#define GAME_SERVER_SRC_SERVICE_PLAYER_SERVICE_H_
#include <memory>
#include <unordered_map>

#include "google/protobuf/message.h"
#include "google/protobuf/service.h"

#include "src/game_logic/game_registry.h"


class PlayerService
{
public:
	PlayerService(::google::protobuf::Service* pb_service)
		: pb_service_(pb_service)
	{

	}

	virtual void CallMethod(const ::google::protobuf::MethodDescriptor* method,
		common::EntityPtr& entity,
		const ::google::protobuf::Message* request,
		::google::protobuf::Message* response) = 0;

	::google::protobuf::Service* service() { return pb_service_.get(); }
private:
	std::unique_ptr<::google::protobuf::Service> pb_service_;
};

void InitPlayerServcie();

extern std::unordered_map<std::string, std::unique_ptr<PlayerService>> g_player_services;
extern std::unordered_set<std::string> g_open_player_services;//开放给玩家的服务


#endif // !GAME_SERVER_SRC_SERVICE_PLAYER_SERVICE_H_
