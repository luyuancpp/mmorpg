
#include "player_skill_handler.h"

///<<< BEGIN WRITING YOUR CODE
#include "table/proto/tip/common_error_tip.pb.h"
#include "proto/common/base/tip.pb.h"
#include "scene/combat/skill/system/skill.h"
#include "macros/return_define.h"
#include "proto/common/component/player_skill_comp.pb.h"

///<<< END WRITING YOUR CODE



void SceneSkillClientPlayerHandler::ReleaseSkill(entt::entity player,const ::ReleaseSkillSkillRequest* request,
	::ReleaseSkillSkillResponse* response)
{
///<<< BEGIN WRITING YOUR CODE
	CHECK_PLAYER_REQUEST(request, SkillSystem::ReleaseSkill);
///<<< END WRITING YOUR CODE

}


void SceneSkillClientPlayerHandler::NotifySkillUsed(entt::entity player,const ::SkillUsedS2C* request,
	::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}


void SceneSkillClientPlayerHandler::NotifySkillInterrupted(entt::entity player,const ::SkillInterruptedS2C* request,
	::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}


void SceneSkillClientPlayerHandler::GetSkillList(entt::entity player,const ::GetSkillListRequest* request,
	::GetSkillListResponse* response)
{
///<<< BEGIN WRITING YOUR CODE
	response->mutable_skill_list()->CopyFrom(tlsRegistryManager.actorRegistry.get_or_emplace<PlayerSkillListPBComponent>(player));
///<<< END WRITING YOUR CODE

}

