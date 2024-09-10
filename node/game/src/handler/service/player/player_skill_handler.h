#pragma once
#include "logic/client_player/player_skill.pb.h"
#include "player_service.h"
#include "macros/return_define.h"
class PlayerSkillServiceHandler : public ::PlayerService
{
public:
	using PlayerService::PlayerService;
	static void ReleaseSkill(entt::entity player,
		const ::ReleaseSkillSkillRequest* request,
		::ReleaseSkillResponse* response);

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
			ReleaseSkill(player,
			::google::protobuf::internal::DownCast<const ReleaseSkillSkillRequest*>(request),
			::google::protobuf::internal::DownCast<ReleaseSkillResponse*>(response));
			HANDLE_ERROR_MESSAGE(::google::protobuf::internal::DownCast<ReleaseSkillResponse*>(response));
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
