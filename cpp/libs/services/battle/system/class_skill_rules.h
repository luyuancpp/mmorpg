#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>

// 四职业技能的纯规则(docs/design/class-skills.md §5):scene 的学习 / 修习 / 技能书 / 被动属性,
// 与 battle 引擎的等级系数 / 耗蓝 / 回蓝共用这一份,改一处两边同时生效。
//
// 零 ECS、零表依赖:入参都是调用方已经从 Skill 表与玩家组件里取好的数,单测直接喂数字。
// 规则来源是内容基线 catalog.json 的 rules 段,数值本身(倍率、幅度、解锁等级)只在 Skill 表里。

namespace classskill {

// 目标阵营(Skill.target_team)。友方含施法者自己与同队宝宝。
enum class TargetTeam : uint32_t {
    kNone = 0,
    kEnemy = 1,
    kAlly = 2,
    kSelf = 3,
};

// 被动加成的属性(Skill.passive_stat)。
// 唯一消费者:前六项与暴击由 scene 的属性重算吃进派生属性(PlayerAttributeSystem::Recalculate),
// 引擎拿到的是已经算好的快照、不再叠加;治疗加成只有引擎消费(施法者出手时现算)。
enum class PassiveStat : uint8_t {
    kNone = 0,
    kDefense,
    kPhysicalAttack,
    kMagicAttack,
    kMaxHealth,
    kMaxMana,
    kSpeed,
    kCrit,          // 暴击率百分点:passive_percent 0.05 = 5 个百分点
    kOutgoingHeal,  // 造成的治疗量比例
    kCount,
};

struct PassiveStatName {
    PassiveStat stat;
    std::string_view name;  // 表里的写法
};

inline constexpr PassiveStatName kPassiveStatNames[] = {
    {PassiveStat::kDefense, "defense"},
    {PassiveStat::kPhysicalAttack, "physicalAttack"},
    {PassiveStat::kMagicAttack, "magicAttack"},
    {PassiveStat::kMaxHealth, "maxHp"},
    {PassiveStat::kMaxMana, "maxMp"},
    {PassiveStat::kSpeed, "speed"},
    {PassiveStat::kCrit, "crit"},
    {PassiveStat::kOutgoingHeal, "outgoingHeal"},
};
// kNone 不进表:除它以外每一项恰好一个表里的写法
static_assert(sizeof(kPassiveStatNames) / sizeof(kPassiveStatNames[0]) ==
              static_cast<std::size_t>(PassiveStat::kCount) - 1);

// 表里的写法 → 枚举;空串与认不出的写法都是 kNone(认不出由启动表校验报错,这里不打日志)
constexpr PassiveStat ParsePassiveStat(std::string_view name) {
    for (const auto& entry : kPassiveStatNames) {
        if (entry.name == name) {
            return entry.stat;
        }
    }
    return PassiveStat::kNone;
}

// 某一重的等级上限:未到解锁等级为 0;之后每 2 级多开一重,封顶 maxRank。
// 等价于内容基线的"修第 n 重需要角色等级 unlockLevel + 2 × (n − 1)"。
constexpr uint32_t RankLimit(uint32_t playerLevel, uint32_t unlockLevel, uint32_t maxRank) {
    if (maxRank == 0 || playerLevel < unlockLevel) {
        return 0;
    }
    return std::min<uint32_t>(maxRank, 1u + (playerLevel - unlockLevel) / 2u);
}

// 实际生效的等级:存档里的等级被当前等级上限夹住(GM 降级 / 回档后不会带着超限等级进战斗)
constexpr uint32_t EffectiveRank(uint32_t storedRank, uint32_t playerLevel, uint32_t unlockLevel,
                                 uint32_t maxRank) {
    return std::min(storedRank, RankLimit(playerLevel, unlockLevel, maxRank));
}

// 修到第 nextRank 重的价格(银两)= 100 × n² + 10 × 解锁等级。n 最大 10,远不会溢出
constexpr uint64_t UpgradeCost(uint32_t nextRank, uint32_t unlockLevel) {
    return 100ull * nextRank * nextRank + 10ull * unlockLevel;
}

// 等级系数。rank 按 [1, ∞) 计,传 0(没学)按 1 算 —— 调用方先保证技能已学才会走到这里
constexpr double LinearRankFactor(uint32_t rank, double growthPerRank) {
    return 1.0 + growthPerRank * static_cast<double>(rank > 0 ? rank - 1 : 0);
}
// 主动伤害 / 治疗:每重 +2% 基础值
constexpr double DamageRankFactor(uint32_t rank) { return LinearRankFactor(rank, 0.02); }
// 主动辅助(增减益幅度、回蓝比例):每重 +2%
constexpr double SupportRankFactor(uint32_t rank) { return LinearRankFactor(rank, 0.02); }
// 被动:每重 +5%
constexpr double PassiveRankFactor(uint32_t rank) { return LinearRankFactor(rank, 0.05); }

// 比例按百万分之一取整后用整数算 value × ratio。配表比例都是有限位小数,直接浮点相乘会因为 0.07 这类数
// 在二进制里略大于本值,把本该是整数的结果多进一位(7.000000000000001 向上取整成 8);
// 整数算法在任何平台上结果都一样,回合引擎的确定性也靠这一点。
inline constexpr uint64_t kRatioScale = 1000000;

// ratio → 百万分之几;非有限 / 非正得 0,大到乘法可能溢出的(> 1800 万倍)夹到能算的上限
inline uint64_t RatioToPpm(double ratio) {
    if (!(ratio > 0.0) || !std::isfinite(ratio)) {
        return 0;
    }
    constexpr uint64_t kMaxPpm = std::numeric_limits<uint64_t>::max() / kRatioScale;
    const double scaled = std::round(ratio * static_cast<double>(kRatioScale));
    return scaled >= static_cast<double>(kMaxPpm) ? kMaxPpm : static_cast<uint64_t>(scaled);
}

// value × ppm / kRatioScale,roundUp 决定余数向上还是向下取整;越过 uint64 上限的夹到上限
inline uint64_t MulPpm(uint64_t value, uint64_t ppm, bool roundUp) {
    constexpr uint64_t kMax = std::numeric_limits<uint64_t>::max();
    const uint64_t quotient = value / kRatioScale;
    const uint64_t remainder = value % kRatioScale;  // < 10^6,乘 ppm(<= kMaxPpm)不溢出
    if (ppm != 0 && quotient > kMax / ppm) {
        return kMax;
    }
    const uint64_t whole = quotient * ppm;
    const uint64_t tail = (remainder * ppm + (roundUp ? kRatioScale - 1 : 0)) / kRatioScale;
    return tail > kMax - whole ? kMax : whole + tail;
}

// ceil(value × ratio);非有限 / 非正比例得 0
inline uint64_t CeilRatio(uint64_t value, double ratio) {
    return MulPpm(value, RatioToPpm(ratio), true);
}

// floor(value × ratio);非有限 / 非正比例得 0(属性按增减益倍率缩放用)
inline uint64_t FloorRatio(uint64_t value, double ratio) {
    return MulPpm(value, RatioToPpm(ratio), false);
}

// 耗蓝 = ceil(最大法力 × mana_percent);比例 > 0 时至少 1(0 法力上限的单位也付得起 1 点就放不出)
inline uint64_t ManaCost(uint64_t maxMana, double manaPercent) {
    if (!(manaPercent > 0.0)) {
        return 0;
    }
    return std::max<uint64_t>(1, CeilRatio(maxMana, manaPercent));
}

// 修习停在哪里(UpgradeSkill 的部分成功原因;kNone = 按请求次数全部修成)
enum class UpgradeStop : uint8_t {
    kNone = 0,
    kMaxRank,     // 已满重
    kLevelLimit,  // 角色等级不够开下一重
    kGold,        // 余额付不起下一重
    kCount,
};

struct UpgradePlan {
    uint32_t steps = 0;       // 实际修成几重
    uint32_t finalRank = 0;   // 修完后的等级
    uint64_t totalCost = 0;   // 总价
    UpgradeStop stop = UpgradeStop::kNone;
};

// 逐重往上修,直到修够 count 重或碰到满重 / 等级上限 / 余额。纯计算,不扣钱。
// 余额按"扣完前面几重之后剩下的"判,所以 10 次修习不会超支。
constexpr UpgradePlan PlanUpgrade(uint32_t currentRank, uint32_t maxRank, uint32_t rankLimit,
                                  uint32_t unlockLevel, uint64_t balance, uint32_t count) {
    UpgradePlan plan;
    plan.finalRank = currentRank;
    while (plan.steps < count) {
        if (plan.finalRank >= maxRank) {
            plan.stop = UpgradeStop::kMaxRank;
            break;
        }
        if (plan.finalRank >= rankLimit) {
            plan.stop = UpgradeStop::kLevelLimit;
            break;
        }
        const uint64_t cost = UpgradeCost(plan.finalRank + 1, unlockLevel);
        if (cost > balance - plan.totalCost) {
            plan.stop = UpgradeStop::kGold;
            break;
        }
        plan.totalCost += cost;
        ++plan.finalRank;
        ++plan.steps;
    }
    return plan;
}

// 一次请求只收 1 次或 10 次
constexpr bool IsValidUpgradeCount(uint32_t count) { return count == 1 || count == 10; }

}  // namespace classskill
