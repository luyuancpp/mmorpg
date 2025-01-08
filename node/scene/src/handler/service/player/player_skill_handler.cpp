#include "player_skill_handler.h"
///<<< BEGIN WRITING YOUR CODE
#include "common_error_tip.pb.h"
#include "proto/common/tip.pb.h"
#include "game_logic/combat/skill/system/skill_system.h"
#include "macros/return_define.h"
#include "proto/logic/component/player_skill_comp.pb.h"
#include "thread_local/storage.h"
///<<< END WRITING YOUR CODE
void PlayerSkillServiceHandler::ReleaseSkill(entt::entity player,const ::ReleaseSkillSkillRequest* request,
	     ReleaseSkillSkillResponse* response)
{
///<<< BEGIN WRITING YOUR CODE
	CHECK_PLAYER_REQUEST(request, SkillSystem::ReleaseSkill);
///<<< END WRITING YOUR CODE
}

void PlayerSkillServiceHandler::NotifySkillUsed(entt::entity player,const ::SkillUsedS2C* request,
	     Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void PlayerSkillServiceHandler::NotifySkillInterrupted(entt::entity player,const ::SkillInterruptedS2C* request,
	     Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void PlayerSkillServiceHandler::GetSkillList(entt::entity player,const ::GetSkillListRequest* request,
	     GetSkillListResponse* response)
{
///<<< BEGIN WRITING YOUR CODE
	response->mutable_skill_list()->CopyFrom(tls.registry.get<PlayerSkillListPBComponent>(player));
///<<< END WRITING YOUR CODE
}

