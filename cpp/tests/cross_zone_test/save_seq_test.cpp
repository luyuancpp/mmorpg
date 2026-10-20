#include <gtest/gtest.h>

#include <cstdint>
#include <optional>

#include "core/utils/proto/proto_dirty_compare.h"
#include "engine/core/type_define/player_guid_comp.h"
#include "engine/core/type_define/type_define.h"
#include "modules/bag/comp/player_bags_comp.h"
#include "modules/currency/comp/player_currency_comp.h"
#include "modules/mission/comp/mission_comp.h"
#include "player/comp/last_persisted_snapshot_comp.h"
#include "player/system/db_task_dispatch_system.h"
#include "player/system/dirty_save_stats.h"
#include "player/system/player_data_loader.h"
#include "player/system/player_persist_marshal.h"
#include "player/comp/player_persist_carry_comp.h"
#include "player/system/player_lifecycle.h"
#include "player/system/save_seq.h"
#include "player/system/save_seq_floor.h"
#include "proto/common/component/actor_comp.pb.h"
#include "proto/common/component/asset_op_ledger_comp.pb.h"
#include "proto/common/component/battle_settlement_ledger_comp.pb.h"
#include "proto/common/component/currency_comp.pb.h"
#include "proto/common/component/player_attribute_comp.pb.h"
#include "proto/common/component/player_comp.pb.h"
#include "proto/common/component/player_inbox_ledger_comp.pb.h"
#include "proto/common/component/player_pet_comp.pb.h"
#include "proto/common/component/player_skill_comp.pb.h"
#include "proto/common/database/player_cache.pb.h"
#include "thread_context/ecs_context.h"
#include "type_alias/player_session_type_alias.h" // SessionMap

// ---------------------------------------------------------------------------
// save_seq_test —— 玩家存盘序号 save_seq 的发号与「序号不污染相等比较」(docs/design/db-write-fencing-mysql.md §7.5)。
//
// 两层:
//   SaveSeq.*(纯)   局部 save_seq::SaveSeqAllocator 实例与纯函数,时间显式传入,不碰 tlsIdGeneratorManager、不读墙钟;
//   SaveSeq.*(ECS)  驱动 PlayerLastPersistedSnapshotComp::Replace、HandlePlayerAsyncSaved 与 SavePlayerToRedis 的快路径。
//                    宿主里没有 Redis 客户端(GetPlayerDataRedis() 为空):快路径用例先 ASSERT 快照相等再调 SavePlayerToRedis,
//                    实现出错时在断言处失败返回,不会走到写盘段对空指针解引用、连带后面的用例不跑。
// InitPlayerFromAllData 里的播种与 SavePlayerToRedisImpl 写盘段的发号在宿主里跑不通(§7.2、§7.5),由代码评审与本地端到端覆盖。
// 用例名统一带 SaveSeq 前缀,§11 第 6 步按名逐条核对。
// ---------------------------------------------------------------------------

namespace
{
using save_seq::ApplyPersonalFloor;
using save_seq::kMaxSaveSeq;
using save_seq::kWallClockShift;
using save_seq::NextSaveSeq;
using save_seq::ObserveResult;
using save_seq::ObserveUpperBound;
using save_seq::SaveSeqAllocator;
using save_seq::SaveSeqFloorFrom;
using save_seq::WallClockFloor;

// 用例里的「现在」:固定的毫秒时间戳(2026-10-16 前后),不读墙钟。
constexpr uint64_t kNowMs = 1'792'000'000'000ULL;
} // namespace

// ============================================================================
// 1. 纯发号
// ============================================================================

TEST(SaveSeq, NextMonotonic)
{
    SaveSeqAllocator allocator;
    uint64_t previous = 0;
    for (uint64_t step = 0; step < 16; ++step)
    {
        const auto seq = allocator.Next(kNowMs + step * 7);
        ASSERT_TRUE(seq.has_value());
        EXPECT_GT(*seq, previous) << "step=" << step;
        EXPECT_EQ(allocator.Current(), *seq) << "Current 是最近一次发出的值";
        previous = *seq;
    }
}

TEST(SaveSeq, NextHonorsWallClockFloor)
{
    SaveSeqAllocator allocator;
    const auto first = allocator.Next(kNowMs);
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(*first, kNowMs << kWallClockShift) << "新进程的第一个号就是墙钟下限";

    // 种子远低于墙钟(进程重启后从落后的父键播种):下一个号仍不低于墙钟下限,越过旧进程可能已发出的序号(§7.4)。
    SaveSeqAllocator restarted;
    EXPECT_EQ(restarted.Observe(12345, kNowMs), ObserveResult::kAdopted);
    const auto afterSeed = restarted.Next(kNowMs + 1000);
    ASSERT_TRUE(afterSeed.has_value());
    EXPECT_EQ(*afterSeed, (kNowMs + 1000) << kWallClockShift);

    EXPECT_EQ(WallClockFloor(kNowMs), kNowMs << kWallClockShift);
    EXPECT_EQ(WallClockFloor(kMaxSaveSeq), kMaxSaveSeq) << "左移会溢出时饱和,不回绕";
}

TEST(SaveSeq, NextMonotonicUnderClockRollback)
{
    SaveSeqAllocator allocator;
    const auto beforeRollback = allocator.Next(kNowMs);
    ASSERT_TRUE(beforeRollback.has_value());
    // 墙钟回拨 10 分钟:last + 1 兜住,不退回更小的墙钟下限。
    const auto afterRollback = allocator.Next(kNowMs - 600'000);
    ASSERT_TRUE(afterRollback.has_value());
    EXPECT_EQ(*afterRollback, *beforeRollback + 1);
    const auto later = allocator.Next(kNowMs - 599'000);
    ASSERT_TRUE(later.has_value());
    EXPECT_EQ(*later, *afterRollback + 1);
}

TEST(SaveSeq, NextSameMillisecond)
{
    SaveSeqAllocator allocator;
    const uint64_t floor = kNowMs << kWallClockShift;
    for (uint64_t i = 0; i < 2048; ++i)
    {
        const auto seq = allocator.Next(kNowMs);
        ASSERT_TRUE(seq.has_value());
        EXPECT_EQ(*seq, floor + i) << "同一毫秒内逐个 +1,超过 1024 个也不撞号";
    }
    const auto nextMs = allocator.Next(kNowMs + 1);
    ASSERT_TRUE(nextMs.has_value());
    EXPECT_EQ(*nextMs, floor + 2048) << "上一毫秒借走的号超过了下一毫秒的墙钟下限:last + 1 优先";
}

TEST(SaveSeq, ObserveRaisesFloor)
{
    SaveSeqAllocator allocator;
    const uint64_t seed = (kNowMs + 30'000) << kWallClockShift; // 别的进程半分钟后发出的号,在容差之内
    EXPECT_EQ(allocator.Observe(seed, kNowMs), ObserveResult::kAdopted);
    EXPECT_EQ(allocator.Current(), seed);
    const auto next = allocator.Next(kNowMs);
    ASSERT_TRUE(next.has_value());
    EXPECT_EQ(*next, seed + 1);

    // 更小的种子不让水位倒退。
    EXPECT_EQ(allocator.Observe(seed - 100, kNowMs), ObserveResult::kAdopted);
    EXPECT_EQ(allocator.Current(), seed + 1);
}

TEST(SaveSeq, ObserveRejectsBeyondBound)
{
    const uint64_t bound = ObserveUpperBound(kNowMs);
    EXPECT_EQ(bound, (kNowMs + save_seq::kObserveToleranceMs) << kWallClockShift);

    SaveSeqAllocator allocator;
    const auto issued = allocator.Next(kNowMs);
    ASSERT_TRUE(issued.has_value());

    EXPECT_EQ(allocator.Observe(bound + 1, kNowMs), ObserveResult::kBeyondBound);
    EXPECT_EQ(allocator.Current(), *issued) << "超界的种子不抬进程水位";
    EXPECT_EQ(allocator.Observe(kMaxSaveSeq - 2, kNowMs), ObserveResult::kBeyondBound) << "近 2^64 的坏数据";
    EXPECT_EQ(allocator.Current(), *issued);

    EXPECT_EQ(allocator.Observe(bound, kNowMs), ObserveResult::kAdopted) << "恰好等于上界算不超界";
    EXPECT_EQ(allocator.Current(), bound);
}

TEST(SaveSeq, NextSaturatesWithoutWrap)
{
    EXPECT_FALSE(NextSaveSeq(kMaxSaveSeq, kNowMs).has_value());
    EXPECT_EQ(NextSaveSeq(kMaxSaveSeq - 1, 0).value_or(0), kMaxSaveSeq);

    // 只有墙钟坏到上界本身饱和时,Observe 才可能把水位抬到 UINT64_MAX;此后 Next 显式失败、水位不变。
    SaveSeqAllocator allocator;
    EXPECT_EQ(allocator.Observe(kMaxSaveSeq, kMaxSaveSeq), ObserveResult::kAdopted);
    EXPECT_FALSE(allocator.Next(kNowMs).has_value());
    EXPECT_EQ(allocator.Current(), kMaxSaveSeq) << "不回绕成 0";
    EXPECT_FALSE(allocator.Next(kNowMs).has_value());
}

TEST(SaveSeq, PersonalFloorBeyondBound)
{
    SaveSeqAllocator allocator;
    const auto before = allocator.Next(kNowMs);
    ASSERT_TRUE(before.has_value());

    // 一台时钟快 1 小时的主机写进父键的序号:超界,只作该玩家的个人下限。
    const uint64_t seed = (kNowMs + 3'600'000) << kWallClockShift;
    EXPECT_EQ(allocator.Observe(seed, kNowMs), ObserveResult::kBeyondBound);
    EXPECT_EQ(allocator.Current(), *before) << "进程水位不变,不污染其他玩家";

    uint64_t personalFloor = seed;
    uint64_t previous = 0;
    for (int i = 0; i < 4; ++i)
    {
        const auto processSeq = allocator.Next(kNowMs + i);
        ASSERT_TRUE(processSeq.has_value());
        const auto issued = ApplyPersonalFloor(*processSeq, personalFloor);
        ASSERT_TRUE(issued.has_value());
        EXPECT_GE(*issued, personalFloor + 1) << "不低于个人下限 + 1";
        EXPECT_GT(*issued, previous) << "同一玩家连续发号严格递增";
        EXPECT_GE(*issued, *processSeq);
        personalFloor = *issued; // 发出后把个人下限推进到发出的值
        previous = *issued;
    }
    EXPECT_LT(allocator.Current(), seed) << "按个人下限发出的号不回写进程水位";

    // 进程水位追上个人下限之后,发号回到进程水位。
    EXPECT_EQ(ApplyPersonalFloor(personalFloor + 50, personalFloor).value_or(0), personalFloor + 50);
}

TEST(SaveSeq, PersonalFloorSaturates)
{
    EXPECT_FALSE(ApplyPersonalFloor(kNowMs << kWallClockShift, kMaxSaveSeq).has_value()) << "显式失败,不回绕";
    EXPECT_EQ(ApplyPersonalFloor(0, kMaxSaveSeq - 1).value_or(0), kMaxSaveSeq);
    EXPECT_EQ(ApplyPersonalFloor(kMaxSaveSeq, 0).value_or(0), kMaxSaveSeq);
}

TEST(SaveSeq, FloorFromAllData)
{
    PlayerAllData empty;
    EXPECT_EQ(SaveSeqFloorFrom(empty), 0u);

    PlayerAllData warm; // 父键存在:只有 last_save_seq
    warm.set_last_save_seq(900);
    EXPECT_EQ(SaveSeqFloorFrom(warm), 900u);

    PlayerAllData cold; // 父键缺失、login 从子表拼出来:只有子表围栏,两张表取大
    cold.mutable_player_database_data()->set_fence_save_seq(700);
    cold.mutable_player_database_1_data()->set_fence_save_seq(800);
    EXPECT_EQ(SaveSeqFloorFrom(cold), 800u);

    PlayerAllData mixed;
    mixed.set_last_save_seq(500);
    mixed.mutable_player_database_data()->set_fence_save_seq(1200);
    mixed.mutable_player_database_1_data()->set_fence_save_seq(1100);
    EXPECT_EQ(SaveSeqFloorFrom(mixed), 1200u) << "三路取 max,不看 epoch";
    mixed.mutable_player_database_data()->set_fence_save_seq(0);
    EXPECT_EQ(SaveSeqFloorFrom(mixed), 1100u);
}

TEST(SaveSeq, PersistedPayloadCurrentIgnoresSeq)
{
    PlayerAllData current;
    current.mutable_player_database_data()->set_player_id(77);
    current.mutable_player_database_1_data()->set_player_id(77);

    PlayerAllData landed = current;
    landed.set_last_save_seq(4242);
    landed.mutable_player_database_data()->set_fence_owner_epoch(3);
    landed.mutable_player_database_data()->set_fence_save_seq(4242);
    landed.mutable_player_database_1_data()->set_fence_owner_epoch(3);
    landed.mutable_player_database_1_data()->set_fence_save_seq(4242);
    EXPECT_TRUE(player_exit::IsPersistedPayloadCurrent(landed, current)) << "只差存盘元数据:判收敛";
    EXPECT_EQ(landed.last_save_seq(), 4242u) << "入参不改";

    PlayerAllData otherSeq = current;
    otherSeq.set_last_save_seq(4243);
    EXPECT_TRUE(player_exit::IsPersistedPayloadCurrent(landed, otherSeq)) << "两笔存盘在途、序号不同:仍判收敛";

    PlayerAllData changed = current;
    changed.mutable_player_database_data()->set_player_id(78);
    EXPECT_FALSE(player_exit::IsPersistedPayloadCurrent(landed, changed)) << "业务字段不同照样判不收敛";
}

TEST(SaveSeq, SnapshotStripsLastSaveSeq)
{
    PlayerAllData landed;
    landed.set_last_save_seq(31337);
    landed.mutable_player_database_data()->set_player_id(77);

    PlayerLastPersistedSnapshotComp comp;
    comp.Replace(landed);
    ASSERT_TRUE(comp.HasSnapshot());
    EXPECT_EQ(comp.snapshot->last_save_seq(), 0u) << "快照不保留 last_save_seq";
    EXPECT_EQ(comp.snapshot->player_database_data().player_id(), 77u) << "业务字段原样保留";
    EXPECT_EQ(landed.last_save_seq(), 31337u) << "入参不改";

    // 已有快照时走赋值分支,同样剥离。
    landed.set_last_save_seq(31338);
    comp.Replace(landed);
    EXPECT_EQ(comp.snapshot->last_save_seq(), 0u);
}

// ============================================================================
// 2. ECS 级:落地回调 → 快照剥离 → 快路径
// ============================================================================

namespace
{
constexpr Guid kSaveSeqPlayerId = 950400001;

// 本宿主不加载配表:在测试创建点显式构造"加载已完成"的持久组件集合(与 cross_zone_test.cpp / db_task_dispatch_test.cpp
// 的 CreatePersistedPlayerComps 同一份清单;那两处都在匿名命名空间里,不跨编译单元复用)。
void CreatePersistedPlayerComps(entt::entity player)
{
    auto& registry = tlsEcs.actorRegistry;
    registry.emplace<Transform>(player);
    registry.emplace<PlayerUint64Comp>(player);
    registry.emplace<PlayerSkillListComp>(player);
    registry.emplace<PlayerUint32Comp>(player);
    registry.emplace<BaseAttributesComp>(player);
    registry.emplace<LevelComp>(player);
    registry.emplace<PlayerAttributeComp>(player);
    registry.emplace<PlayerPetComp>(player);
    registry.emplace<PlayerProfileComp>(player);
    registry.emplace<PlayerBagsComp>(player);
    registry.emplace<MissionsContainerComp>(player);
    registry.emplace<PlayerAssetOpLedgerComp>(player);
    registry.emplace<BattleSettlementLedgerComp>(player);
    registry.emplace<PlayerInboxLedgerComp>(player);
    // 神秘商人限购记录是 2026-10-19 新增的持久组件,这份清单当时漏了;缺它 marshal 会整份拒存。
    registry.emplace<PlayerMysteryMerchantComp>(player);
    registry.emplace<CurrencyComp>(player);
    registry.emplace<PlayerCurrencyComp>(player);
    // 两表顶层未知字段的载体(混版本持久字段保护 §4.2 L1),生产里由两张表的 Unmarshal 创建。
    registry.emplace<PlayerDatabaseCarryComp>(player);
    registry.emplace<PlayerDatabase1CarryComp>(player);
}

// 在线(未退出、无交接)的玩家:落地回调只更新快照,不走退出 / 交接分支。
entt::entity MakeOnlinePlayer()
{
    const auto player = tlsEcs.actorRegistry.create();
    CreatePersistedPlayerComps(player);
    tlsEcs.playerList.emplace(kSaveSeqPlayerId, player);
    tlsEcs.actorRegistry.emplace<PlayerGuidComp>(player, kSaveSeqPlayerId);
    return player;
}

// 与 SavePlayerToRedis 快路径比较时的新消息同一个形状:走同一个统一入口(拼装 + player_id + 持久 schema 世代),
// 不带 last_save_seq。
bool MarshalAsFresh(entt::entity player, PlayerAllData& data)
{
    return player_persist::MarshalPlayerForPersist(player, kSaveSeqPlayerId, data);
}

class SaveSeqEcsFixture : public ::testing::Test
{
protected:
    void SetUp() override
    {
        tlsEcs.Clear();
        SessionMap().clear();
        // 快路径在快照相等时会查 DBTask 投递账本(NeedsResave):用空账本,且不留给 cross_zone_test.cpp 里依赖空账本的用例。
        DbTaskDispatchSystem::ResetForTest();
    }

    void TearDown() override
    {
        DbTaskDispatchSystem::ResetForTest();
        tlsEcs.Clear();
        SessionMap().clear();
    }
};

using SaveSeqEcs = SaveSeqEcsFixture;
} // namespace

TEST_F(SaveSeqEcs, SecondUnchangedSaveHitsFastPath)
{
    const auto player = MakeOnlinePlayer();

    // 第一次存盘落地:Redis 里的那份带着发号写入的 last_save_seq。
    PlayerAllData landed;
    ASSERT_TRUE(MarshalAsFresh(player, landed));
    landed.set_last_save_seq(7001);
    PlayerLifecycleSystem::HandlePlayerAsyncSaved(kSaveSeqPlayerId, landed);

    ASSERT_TRUE(tlsEcs.actorRegistry.valid(player));
    const auto* snap = tlsEcs.actorRegistry.try_get<PlayerLastPersistedSnapshotComp>(player);
    ASSERT_NE(snap, nullptr) << "落地回调记下快照";
    ASSERT_TRUE(snap->HasSnapshot());
    ASSERT_EQ(snap->snapshot->last_save_seq(), 0u) << "快照在 Replace 时剥离了 last_save_seq";

    // 内存没变:新 marshal 与快照逐字段相等(新消息此刻还没发号,两侧都不带序号)。
    PlayerAllData fresh;
    ASSERT_TRUE(MarshalAsFresh(player, fresh));
    ASSERT_TRUE(dirty_save::IsEqual(fresh, *snap->snapshot))
        << "这里不等,下面的 SavePlayerToRedis 会越过快路径去写空的 Redis 客户端;先在断言处失败";

    const auto before = dirty_save_stats::Read();
    EXPECT_FALSE(PlayerLifecycleSystem::SavePlayerToRedis(player)) << "第二次无改动的存盘命中快路径,不写盘";
    const auto after = dirty_save_stats::Read();
    EXPECT_EQ(after.total - before.total, 1u);
    EXPECT_EQ(after.skipped - before.skipped, 1u) << "计入快路径跳过";
}

TEST_F(SaveSeqEcs, FastPathIgnoresDifferingLastSaveSeq)
{
    const auto player = MakeOnlinePlayer();

    // 连续两次落地,序号不同(每次写盘都变),业务内容相同:快路径仍判相等。
    for (const uint64_t landedSeq : {uint64_t{9001}, uint64_t{9005}})
    {
        PlayerAllData landed;
        ASSERT_TRUE(MarshalAsFresh(player, landed));
        landed.set_last_save_seq(landedSeq);
        PlayerLifecycleSystem::HandlePlayerAsyncSaved(kSaveSeqPlayerId, landed);

        const auto* snap = tlsEcs.actorRegistry.try_get<PlayerLastPersistedSnapshotComp>(player);
        ASSERT_NE(snap, nullptr);
        ASSERT_TRUE(snap->HasSnapshot());
        PlayerAllData fresh;
        ASSERT_TRUE(MarshalAsFresh(player, fresh));
        ASSERT_TRUE(dirty_save::IsEqual(fresh, *snap->snapshot)) << "landed_seq=" << landedSeq;

        EXPECT_FALSE(PlayerLifecycleSystem::SavePlayerToRedis(player)) << "landed_seq=" << landedSeq;
    }
}
