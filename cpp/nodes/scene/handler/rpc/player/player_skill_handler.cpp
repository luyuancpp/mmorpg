
#include "player_skill_handler.h"

///<<< BEGIN WRITING YOUR CODE
#include "table/proto/tip/common_error_tip.pb.h"
#include "proto/common/base/tip.pb.h"
#include "combat/skill/system/skill.h"
#include "macros/return_define.h"
#include "proto/common/component/player_skill_comp.pb.h"
#include "player/system/player_skill.h"
#include "table/code/skill_table.h"
#include <thread_context/ecs_context.h>

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
	if (const auto* skillList = tlsEcs.actorRegistry.try_get<PlayerSkillListComp>(player);
		skillList != nullptr)
	{
		// 只下发本版本生效的技能:列表里可能留着新版本技能表才有的技能,它们保留但不生效
		// (PlayerSkillSystem::IsActiveSkill,混版本设计 §12 第 5 批)。下发了客户端也施放不了(ReleaseSkill 按表拒)。
		auto* out = response->mutable_skill_list();
		for (const auto& skill : skillList->skill_list())
		{
			if (PlayerSkillSystem::IsActiveSkill(skill.skill_table_id()))
			{
				out->add_skill_list()->CopyFrom(skill);
			}
		}
		return;
	}
	// Force the skill_list field to be present (even if empty) so the
	// client/robot can distinguish "we replied with no skills yet" from
	// "the response was dropped". Robot's
	// SceneSkillClientPlayerListSkillsHandler treats a nil SkillList as
	// "skills cleared" via SetOwnedSkillIDs(nil)+SignalSkillsReady, so an
	// explicit empty list keeps that signal firing on first login. Do not
	// remove this line.
	response->mutable_skill_list();
///<<< END WRITING YOUR CODE

}
