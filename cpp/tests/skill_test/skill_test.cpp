#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "combat/skill/system/skill.h"

#include "proto/scene/player_skill.pb.h"
#include "table/code/skill_table.h"
#include "combat/skill/comp/skill_comp.h"
#include "combat/skill/constants/skill.h"
#include "time/comp/timer_task_comp.h"
#include "time/system/time_cooldown.h"
#include "table/proto/tip/skill_error_tip.pb.h"
#include "table/proto/tip/common_error_tip.pb.h"
#include "player/system/player_skill.h"
#include "player/comp/player_frozen_comp.h"
#include "modules/scene/comp/scene_comp.h"
#include "proto/common/component/player_comp.pb.h"
#include "macros/return_define.h"
#include <thread_context/ecs_context.h>
#include "../test_config_helper.h"

using ::testing::_;
using ::testing::Return;

// Mocked classes and functions
class MockSkillTable {
public:
    MOCK_METHOD(const SkillTable*, GetSkillTable, (uint32_t), (const));
    MOCK_METHOD(bool, IsSkillOfType, (uint32_t, uint32_t), (const));
};

class MockCooldownTimeUtil {
public:
    MOCK_METHOD(bool, IsInCooldown, (const CooldownTimeComp&), (const));
};

// Test Fixture
class SkillUtilTest : public ::testing::Test {
protected:
    void SetUp() override {
        skillUtil = std::make_unique<SkillSystem>();
        // Setup the mocks if necessary
    }

    void TearDown() override {
        tlsEcs.actorRegistry.clear(); // Clean up the thread-local storage after each test
        tlsEcs.sceneRegistry.clear();
    }

    // 放进指定场景的玩家:ResolveSkillTarget 要求目标是 Player/Npc 且与施法者同场景。
    static entt::entity CreatePlayerInScene(entt::entity scene) {
        const auto player = tlsEcs.actorRegistry.create();
        tlsEcs.actorRegistry.emplace<Player>(player);
        tlsEcs.actorRegistry.emplace<SceneEntityComp>(player, SceneEntityComp{ scene });
        return player;
    }

    static ::ReleaseSkillRequest TargetedRequest(entt::entity target) {
        ::ReleaseSkillRequest request;
        request.set_skill_table_id(1); // 1 号技能是指定目标类(targeting_mode 首项为 1 → kTargetedSkill)
        request.set_target_id(entt::to_integral(target));
        return request;
    }

    std::unique_ptr<SkillSystem> skillUtil;
    std::unique_ptr<MockSkillTable> mockSkillTable = std::make_unique<MockSkillTable>();
    std::unique_ptr<MockCooldownTimeUtil> mockCooldownTimeUtil = std::make_unique<MockCooldownTimeUtil>();
};

TEST_F(SkillUtilTest, ValidateTarget_InvalidTarget_ReturnsError) {
    ::ReleaseSkillRequest request;
    request.set_skill_table_id(1);
    request.set_target_id(-1); // Invalid target ID

    EXPECT_CALL(*mockSkillTable, GetSkillTable(request.skill_table_id()))
        .WillRepeatedly(Return(nullptr)); // Mock implementation

    const entt::entity caster = tlsEcs.actorRegistry.create();
    uint32_t result = skillUtil->ValidateTarget(caster, &request);
    EXPECT_EQ(result, kSkillInvalidTargetId);
}

TEST_F(SkillUtilTest, ValidateTarget_ValidTarget_ReturnsOk) {
    entt::entity target = tlsEcs.actorRegistry.create(); // Create a valid target in the registry

    ::ReleaseSkillRequest request;
    request.set_skill_table_id(10);
    request.set_target_id(entt::to_integral(target)); // Valid target ID

    SkillTable tableSkill;
    tableSkill.mutable_targeting_mode()->Add(1); // Add target type to simulate need for target

    EXPECT_CALL(*mockSkillTable, GetSkillTable(request.skill_table_id()))
        .WillRepeatedly(Return(&tableSkill));

    const entt::entity caster = tlsEcs.actorRegistry.create();
    uint32_t result = skillUtil->ValidateTarget(caster, &request);
    EXPECT_EQ(result, kSuccess);
}

// actorRegistry 由本节点所有场景共用:客户端填别的场景里玩家的实体整数,
// valid() 照样为真。指定目标类技能必须拒绝不在施法者场景里的目标。
TEST_F(SkillUtilTest, ValidateTarget_TargetInAnotherScene_ReturnsError) {
    const auto casterScene = tlsEcs.sceneRegistry.create();
    const auto otherScene = tlsEcs.sceneRegistry.create();
    const auto caster = CreatePlayerInScene(casterScene);
    const auto sameSceneTarget = CreatePlayerInScene(casterScene);
    const auto otherSceneTarget = CreatePlayerInScene(otherScene);

    auto sameSceneRequest = TargetedRequest(sameSceneTarget);
    EXPECT_EQ(kSuccess, skillUtil->ValidateTarget(caster, &sameSceneRequest));

    auto otherSceneRequest = TargetedRequest(otherSceneTarget);
    EXPECT_EQ(kSkillInvalidTargetId, skillUtil->ValidateTarget(caster, &otherSceneRequest));
}

TEST_F(SkillUtilTest, ValidateTarget_FrozenTarget_ReturnsError) {
    const auto scene = tlsEcs.sceneRegistry.create();
    const auto caster = CreatePlayerInScene(scene);
    const auto target = CreatePlayerInScene(scene);
    tlsEcs.actorRegistry.emplace<PlayerFrozenComp>(target);

    auto request = TargetedRequest(target);
    EXPECT_EQ(kSkillInvalidTargetId, skillUtil->ValidateTarget(caster, &request));
}

// 对自己施放的技能结束时,施法者表里的这一条只能删一次(以前先按 key 删、再用失效迭代器删)。
TEST_F(SkillUtilTest, HandleSkillFinish_SelfTargetedContextIsErasedOnce) {
    const auto caster = tlsEcs.actorRegistry.create();
    auto& contexts = tlsEcs.actorRegistry.emplace<SkillContextCompMap>(caster);
    auto context = std::make_shared<SkillContextComp>();
    context->set_skillid(42);
    context->set_caster(entt::to_integral(caster));
    context->set_target(entt::to_integral(caster));
    contexts.emplace(context->skillid(), context);

    SkillSystem::HandleSkillFinish(caster, 42);

    EXPECT_TRUE(tlsEcs.actorRegistry.get<SkillContextCompMap>(caster).empty());
}

// 冷却表缺失时必须拒绝施法;以前静默放行,冷却从未生效。
TEST_F(SkillUtilTest, CheckCooldown_MissingCooldownComp_ReturnsError) {
    const entt::entity caster = tlsEcs.actorRegistry.create();
    SkillTable tableSkill;
    tableSkill.set_cooldown_id(1);

    EXPECT_EQ(kServiceUnavailable, skillUtil->CheckCooldown(caster, &tableSkill));
}

// handler 直接写进 response 的拒绝码(ReleaseSkill 就是这么写的)不能被空的 TLS tip 覆盖成「成功」。
TEST(TransferErrorMessageTest, KeepsDirectResponseErrorWhenTlsTipIsEmpty) {
    tlsEcs.globalRegistry.get_or_emplace<TipInfoMessage>(tlsEcs.GlobalEntity()).Clear();
    ::ReleaseSkillResponse response;
    response.mutable_error_message()->set_id(kSkillInvalidTargetId);

    TRANSFER_ERROR_MESSAGE(&response);

    EXPECT_EQ(kSkillInvalidTargetId, response.error_message().id());
}

TEST(TransferErrorMessageTest, MovesTlsTipIntoResponseAndClearsIt) {
    tlsEcs.globalRegistry.get_or_emplace<TipInfoMessage>(tlsEcs.GlobalEntity()).set_id(kSkillCooldownNotReady);
    ::ReleaseSkillResponse response;

    TRANSFER_ERROR_MESSAGE(&response);

    EXPECT_EQ(kSkillCooldownNotReady, response.error_message().id());
    EXPECT_EQ(0u, tlsEcs.globalRegistry.get<TipInfoMessage>(tlsEcs.GlobalEntity()).id());
}

// 全新 EcsContext 的 actorRegistry 已让 0 号句柄退役(协议里 0 表示「没有目标」),
// 第一个 actor 不能再是整数 0。用局部对象而不是 tlsEcs,结果不受其它用例执行顺序影响。
TEST(EcsContextTest, FirstActorEntityIntegralIsNotZero) {
    EcsContext fresh;
    const auto first = fresh.actorRegistry.create();
    EXPECT_NE(0u, entt::to_integral(first));
    EXPECT_FALSE(fresh.actorRegistry.valid(entt::entity{ 0 }));
}

TEST_F(SkillUtilTest, CheckCooldown_CooldownActive_ReturnsError) {
    entt::entity caster = tlsEcs.actorRegistry.create();

    auto tableSkill = std::make_shared<SkillTable>();
    tableSkill->set_cooldown_id(1);

    CooldownTimeComp cooldownTimeComp;
    cooldownTimeComp.set_cooldown_table_id(1);

    EXPECT_CALL(*mockCooldownTimeUtil, IsInCooldown(_))
        .WillRepeatedly(Return(true));

    auto & cooldownList = tlsEcs.actorRegistry.get_or_emplace<CooldownTimeListComp>(caster);
    cooldownTimeComp.set_cooldown_table_id(1);
    CoolDownTimeMillisecondSystem::Reset(cooldownTimeComp);
    cooldownList.mutable_cooldown_list()->emplace(1, cooldownTimeComp);

    uint32_t result = skillUtil->CheckCooldown(caster, tableSkill.get());
    EXPECT_EQ(result, kSkillCooldownNotReady);
}

TEST_F(SkillUtilTest, CheckCooldown_CooldownInactive_ReturnsOk) {
    entt::entity caster = tlsEcs.actorRegistry.create();

    auto tableSkill = std::make_shared<SkillTable>();
    tableSkill->set_cooldown_id(1);

    CooldownTimeComp cooldownTimeComp;
    cooldownTimeComp.set_cooldown_table_id(1);

    EXPECT_CALL(*mockCooldownTimeUtil, IsInCooldown(_))
        .WillRepeatedly(Return(false));

    auto& cooldownList = tlsEcs.actorRegistry.get_or_emplace<CooldownTimeListComp>(caster);
    cooldownList.mutable_cooldown_list()->emplace(1, cooldownTimeComp);

    uint32_t result = skillUtil->CheckCooldown(caster, tableSkill.get());
    EXPECT_EQ(result, kSuccess);
}

TEST_F(SkillUtilTest, HandleCastingTimer_ImmediateSkill_ReturnsOk) {
    entt::entity caster = tlsEcs.actorRegistry.create();
    auto tableSkill = std::make_shared<SkillTable>();
    tableSkill->set_immediate(true);
    tableSkill->set_cast_point(1000); // Set cast point to 1000ms

    EXPECT_CALL(*mockSkillTable, GetSkillTable(_))
        .WillRepeatedly(Return(tableSkill.get()));

    uint32_t result = skillUtil->CheckCasting(caster, tableSkill.get());
    EXPECT_EQ(result, kSuccess);
}

TEST_F(SkillUtilTest, HandleRecoveryTimeTimer_ImmediateSkill_ReturnsOk) {
    entt::entity caster = tlsEcs.actorRegistry.create();
    auto tableSkill = std::make_shared<SkillTable>();
    tableSkill->set_immediate(true);
    tableSkill->set_recovery_time(1000); // Set recovery time to 1000ms

    EXPECT_CALL(*mockSkillTable, GetSkillTable(_))
        .WillRepeatedly(Return(tableSkill.get()));

    uint32_t result = skillUtil->CheckRecovery(caster, tableSkill.get());
    EXPECT_EQ(result, kSuccess);
}

TEST_F(SkillUtilTest, HandleChannelTimeTimer_ImmediateSkill_ReturnsOk) {
    entt::entity caster = tlsEcs.actorRegistry.create();
    auto tableSkill = std::make_shared<SkillTable>();
    tableSkill->set_immediate(true);
    tableSkill->set_channel_finish(1000); // Set channel finish time to 1000ms
    tableSkill->set_channel_think(500);   // Set channel interval to 500ms

    EXPECT_CALL(*mockSkillTable, GetSkillTable(_))
        .WillRepeatedly(Return(tableSkill.get()));

    uint32_t result = skillUtil->CheckChannel(caster, tableSkill.get());
    EXPECT_EQ(result, kSuccess);
}

TEST_F(SkillUtilTest, BroadcastSkillUsedMessage_CreatesMessage) {
    entt::entity caster = tlsEcs.actorRegistry.create();
    ::ReleaseSkillRequest request;
    request.set_skill_table_id(1);
    request.set_target_id(2);
    request.mutable_position()->set_x(10); // Mock position

    EXPECT_CALL(*mockSkillTable, GetSkillTable(request.skill_table_id()))
        .WillRepeatedly(Return(nullptr)); // Mock implementation

    EXPECT_NO_THROW(skillUtil->BroadcastSkillUsedMessage(caster, &request));
}

TEST_F(SkillUtilTest, SetupCastingTimer_SetsTimer) {
    entt::entity caster = tlsEcs.actorRegistry.create();
    auto tableSkill = std::make_shared<SkillTable>();
    tableSkill->set_id(1);
    tableSkill->set_cast_point(1); // Set cast point to 1000ms

    EXPECT_CALL(*mockSkillTable, GetSkillTable(_))
        .WillRepeatedly(Return(tableSkill.get()));

    EXPECT_CALL(*mockSkillTable, IsSkillOfType(_, kGeneralSkill))
        .WillRepeatedly(Return(true));

    skillUtil->SetupCastingTimer(caster, tableSkill.get(), 2);

    auto* castingTimerComp = tlsEcs.actorRegistry.try_get<CastingTimerComp>(caster);
    ASSERT_NE(castingTimerComp, nullptr);  // Check if the component was created
    EXPECT_TRUE(castingTimerComp->timer.IsActive());  // Check if the timer is active
}

TEST_F(SkillUtilTest, HandleSkillSpell_TriggersEffect) {
    entt::entity caster = tlsEcs.actorRegistry.create();


    auto tableSkill = std::make_shared<SkillTable>();
    tableSkill->set_id(1);

    EXPECT_CALL(*mockSkillTable, GetSkillTable(1))
        .WillRepeatedly(Return(tableSkill.get()));

    EXPECT_NO_THROW(skillUtil->HandleGeneralSkillSpell(caster, 1));
}

TEST_F(SkillUtilTest, HandleSkillRecovery_SetsRecoveryTimer) {
    entt::entity caster = tlsEcs.actorRegistry.create();


    auto tableSkill = std::make_shared<SkillTable>();
    tableSkill->set_recovery_time(1000); // Set recovery time to 1000ms

    EXPECT_CALL(*mockSkillTable, GetSkillTable(_))
        .WillRepeatedly(Return(tableSkill.get()));

    EXPECT_NO_THROW(skillUtil->HandleSkillRecovery(caster, 1));
}

TEST_F(SkillUtilTest, SendSkillInterruptedMessage_SendsMessage) {
    entt::entity caster = tlsEcs.actorRegistry.create();
    uint32_t skillId = 1;

    EXPECT_NO_THROW(skillUtil->SendSkillInterruptedMessage(caster, skillId));
}

TEST(PlayerSkillSystemTest, SanitizeSkillListRemovesUnknownAndDuplicateSkills)
{
    entt::entity player = tlsEcs.actorRegistry.create();
    auto &skillList = tlsEcs.actorRegistry.get_or_emplace<PlayerSkillListComp>(player);

    skillList.add_skill_list()->set_skill_table_id(1);
    skillList.add_skill_list()->set_skill_table_id(1001);
    skillList.add_skill_list()->set_skill_table_id(1);

    PlayerSkillSystem::SanitizeSkillList(player);

    ASSERT_EQ(skillList.skill_list_size(), 1);
    EXPECT_EQ(skillList.skill_list(0).skill_table_id(), 1);
    EXPECT_TRUE(PlayerSkillSystem::HasSkill(player, 1));
    EXPECT_FALSE(PlayerSkillSystem::HasSkill(player, 1001));
}

// Main function
int main(int argc, char** argv) {
    if (!test_config::FindAndLoadTestConfig(argc, argv))
        return 1;

    EventLoop loop;

    ::testing::InitGoogleTest(&argc, argv);
    CooldownTableManager::Instance().Load();
    SkillTableManager::Instance().Load();

    int ret = RUN_ALL_TESTS();
    tlsEcs.actorRegistry.clear(); // Clean up thread-local storage after all tests
    return ret;
}
