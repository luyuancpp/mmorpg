
#include "player_skill_handler.h"

///<<< BEGIN WRITING YOUR CODE
#include "table/proto/tip/common_error_tip.pb.h"
#include "proto/common/base/tip.pb.h"
#include "combat/skill/system/skill.h"
#include "macros/return_define.h"
#include "proto/common/component/player_skill_comp.pb.h"
#include "player/system/player_skill.h"
#include "table/code/skill_table.h"

///<<< END WRITING YOUR CODE

void SceneSkillClientPlayerHandler::ReleaseSkill(entt::entity player,const ::ReleaseSkillRequest* request,
	::ReleaseSkillResponse* response)
{
///<<< BEGIN WRITING YOUR CODE
const auto skillTableId = request->skill_table_id();
if (skillTableId == 0 || !SkillTableManager::Instance().Exists(skillTableId))
{
	LOG_WARN << "Reject ReleaseSkill: unknown skill_table_id=" << skillTableId
			 << " entity=" << entt::to_integral(player);
	response->mutable_error_message()->set_id(kInvalidTableId);
	return;
}

if (!PlayerSkillSystem::HasSkill(player, skillTableId))
{
	LOG_WARN << "Reject ReleaseSkill: entity=" << entt::to_integral(player)
			 << " does not own skill_table_id=" << skillTableId;
	response->mutable_error_message()->set_id(kInvalidTableId);
	return;
}

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

void SceneSkillClientPlayerHandler::ListSkills(entt::entity player,const ::ListSkillsRequest* request,
	::ListSkillsResponse* response)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}
