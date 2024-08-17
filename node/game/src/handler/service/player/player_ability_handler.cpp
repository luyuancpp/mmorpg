#include "player_ability_handler.h"
///<<< BEGIN WRITING YOUR CODE
#include "common_error_tip.pb.h"
#include "common/tip.pb.h"
#include "game_logic/combat/ability/util/ability_util.h"
#include "macros/return_define.h"
///<<< END WRITING YOUR CODE
void PlayerAbilityServiceHandler::UseAbility(entt::entity player,const ::UseAbilityRequest* request,
	     UseAbilityResponse* response)
{
///<<< BEGIN WRITING YOUR CODE
	CHECK_PLAYER_REQUEST(request, AbilityUtil::CheckSkillPrerequisites);
///<<< END WRITING YOUR CODE
}

void PlayerAbilityServiceHandler::NotifyAbilityUsed(entt::entity player,const ::AbilityUsedS2C* request,
	     Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void PlayerAbilityServiceHandler::NotifyAbilityInterrupted(entt::entity player,const ::AbilityInterruptedS2C* request,
	     Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

