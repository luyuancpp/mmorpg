#include "player_skill_handler.h"
///<<< BEGIN WRITING YOUR CODE
#include "common_error_tip.pb.h"
#include "common/tip.pb.h"
#include "game_logic/combat/skill/util/skill_util.h"
#include "macros/return_define.h"
///<<< END WRITING YOUR CODE
void PlayerSkillServiceHandler::UseSkill(entt::entity player,const ::UseSkillRequest* request,
	     UseSkillResponse* response)
{
///<<< BEGIN WRITING YOUR CODE
	CHECK_PLAYER_REQUEST(request, SkillUtil::UseSkill);
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

