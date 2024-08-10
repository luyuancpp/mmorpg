#include "player_ability_handler.h"
///<<< BEGIN WRITING YOUR CODE
#include "game_logic/combat/ability/util/ability_util.h"
#include "macros/return_define.h"
#include "common/tip.pb.h"
///<<< END WRITING YOUR CODE
void PlayerAbilityServiceHandler::UseAbility(entt::entity player,const ::UseAbilityRequest* request,
	     UseAbilityResponse* response)
{
///<<< BEGIN WRITING YOUR CODE
	AbilityUtil::CheckSkillActivationPrerequisites(request);

	HANDLE_ERROR_MESSAGE(response);

///<<< END WRITING YOUR CODE
}

