#pragma once

#include <cstdint>

#include <entt/src/entt/entity/entity.hpp>

class BattlePlayerSnapshot;
class ListSkillsResponse;
class SkillTable;
class UpgradeSkillRequest;
class UpgradeSkillResponse;

// 角色技能(docs/design/class-skills.md)。PlayerSkillListComp 的唯一写入口。
//
// 两类技能共用一张列表、按 Skill.class_id 区分:
//   * 职业技能(class_id = 1..4,四职业 32 个):有等级。列表里只有已学的(rank >= 1),未学的不占条目;
//     职业、解锁等级、等级上限、价格全由 Skill 表 + classskill:: 纯规则决定,客户端只看服务端算好的技能书。
//   * 旧通用技能(class_id = 0,ID 1..14 的占位技能):不分等级,rank 恒 0。只留给还没有职业技能的职业(5..9)。
//
// 组件归属:PlayerSkillListComp 由加载(PlayerDatabaseMessageFieldsUnmarshal)挂上;职业(PlayerUint32Comp)
// 与等级(LevelComp)同样由加载挂上。本类只取不建,缺组件按服务端故障处理。
// 写操作只发生在 scene 线程(EventLoop),扣费与改等级在同一个回调里完成、同属一条 player_database 记录,
// 由常规存盘整份落库:不存在"钱扣了、等级没涨"或反过来的中间态(与属性洗点 Reset 同一口径)。
class PlayerSkillSystem {
public:
	// 新号创建入口:按当前职业补初始技能。与 InitializeOnLoad 共用同一段逻辑,幂等。
	static void RegisterPlayer(entt::entity player);

	// 加载期整理,必须在职业与等级组件挂上之后、属性重算(PlayerAttributeSystem::InitializeOnLoad)之前调用:
	// 先 SanitizeSkillList,再按 Class.skill 补初始技能(职业技能 1 重)。被动加成随后由属性重算吃进去。
	static void InitializeOnLoad(entt::entity player);

	// 清理存档里的技能列表(加载期与单测用),规则见 class-skills.md §3.2:
	//   * 表里没有的 ID、重复记录:剔除;
	//   * 职业技能:不属于当前职业(转职 / 错职业)或 rank 为 0 的剔除;rank 超过表上限的夹到上限;
	//   * 旧通用技能:当前职业已有职业技能时剔除(那是旧 RegisterPlayer 遍历所有职业种子发下的占位技能),
	//     否则保留。
	// 每剔除 / 修正一条打一行 WARN,不静默。
	static void SanitizeSkillList(entt::entity player);

	// 是否拥有且能用:旧通用技能在列表里即可;职业技能还要生效等级 > 0(职业对、等级够)
	static bool HasSkill(entt::entity player, uint32_t skillTableId);

	// 职业技能的生效等级 = min(存档等级, 当前角色等级的上限);没学 / 不是本职业 / 旧通用技能返回 0
	static uint32_t EffectiveRank(entt::entity player, uint32_t skillTableId);

	// 技能书(ListSkills / UpgradeSkill 的回包):已拥有列表 + 当前职业完整目录 + 等级 + 余额。
	// 返回 kSuccess;实体无效 kEntityIsNull;缺必需组件 kServiceUnavailable(此时 response 只有 error 以外的空值)。
	static uint32_t BuildSkillBook(entt::entity player, ListSkillsResponse& response);

	// 修习(学习 = 0 → 1)。整笔拒绝时填 error_message 且不扣钱、不改等级;部分成功时 upgraded_count < count、
	// stop_tip_id 说明停在哪。无论成败 state 都是处理之后的完整技能书。
	static void UpgradeSkill(entt::entity player, const UpgradeSkillRequest& request, UpgradeSkillResponse& response);

	// 战斗快照:skill_table_ids 放回合制能施放的(旧通用技能 + 已学的职业主动技能),
	// skill_ranks 放全部已学职业技能(含被动)的生效等级。
	static void FillBattleSnapshot(entt::entity player, BattlePlayerSnapshot& snapshot);

	// 被动加成之和(给属性重算用;scene 是这几项被动唯一的消费者,引擎不再叠加)。
	// 比例项 0.08 = +8%;critPercentPoints 是暴击率百分点(5.0 = +5 个百分点)。
	struct PassiveBonus {
		double defense = 0.0;
		double physicalAttack = 0.0;
		double magicAttack = 0.0;
		double maxHealth = 0.0;
		double maxMana = 0.0;
		double speed = 0.0;
		double critPercentPoints = 0.0;
	};
	static PassiveBonus CollectPassiveBonus(entt::entity player);

	// 行分类(只看表,不看玩家)
	static bool IsClassSkill(const SkillTable& row);
	static bool IsPassiveSkill(const SkillTable& row);
	// 回合制能不能施放:被动(passive_stat 非空)、以及老行的被动 / 开关 / 持续施法位都不行。
	// 与引擎 TurnBattleEngine::IsTurnBattleCastableSkill 同一判据,两个进程各守一道。
	static bool IsTurnBattleCastable(const SkillTable& row);
};
