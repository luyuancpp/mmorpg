#pragma once
#include "logic/client_player/player_skill.pb.h"
#include "player_service.h"
#include "macros/return_define.h"
class PlayerSkillServiceHandler : public ::PlayerService
{
public:
	using PlayerService::PlayerService;
	static void UseSkill(entt::entity player,
		const ::UseSkillRequest* request,
		::UseSkillResponse* response);

	static void NotifySkillUsed(entt::entity player,
		const ::SkillUsedS2C* request,
		::Empty* response);

	static void NotifySkillInterrupted(entt::entity player,
		const ::SkillInterruptedS2C* request,
		::Empty* response);

	void CallMethod(const ::google::protobuf::MethodDescriptor* method,
		entt::entity player,
		const ::google::protobuf::Message* request,
		::google::protobuf::Message* response)override 
		{
		switch(method->index())
		{
		case 0:
			UseSkill(player,
			::google::protobuf::internal::DownCast<const UseSkillRequest*>(request),
			::google::protobuf::internal::DownCast<UseSkillResponse*>(response));
			HANDLE_ERROR_MESSAGE(::google::protobuf::internal::DownCast<UseSkillResponse*>(response));
		break;
		case 1:
			NotifySkillUsed(player,
			::google::protobuf::internal::DownCast<const SkillUsedS2C*>(request),
			::google::protobuf::internal::DownCast<Empty*>(response));
		break;
		case 2:
			NotifySkillInterrupted(player,
			::google::protobuf::internal::DownCast<const SkillInterruptedS2C*>(request),
			::google::protobuf::internal::DownCast<Empty*>(response));
		break;
		default:
		break;
		}
	}

};
