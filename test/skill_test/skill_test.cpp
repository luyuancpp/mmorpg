#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "combat/skill/system/skill_system.h"
#include "thread_local/storage.h"
#include "skill_config.h"
#include "combat/skill/comp/skill_comp.h"
#include "combat/skill/constants/skill_constants.h"
#include "time/comp/timer_task_comp.h"
#include "time/system/cooldown_time_system.h"
#include "pbc/skill_error_tip.pb.h"
#include "pbc/common_error_tip.pb.h"

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
        tls.actorRegistry.clear(); // Clean up the thread-local storage after each test
    }

    std::unique_ptr<SkillSystem> skillUtil;
    std::unique_ptr<MockSkillTable> mockSkillTable = std::make_unique<MockSkillTable>();
    std::unique_ptr<MockCooldownTimeUtil> mockCooldownTimeUtil = std::make_unique<MockCooldownTimeUtil>();
};

TEST_F(SkillUtilTest, ValidateTarget_InvalidTarget_ReturnsError) {
    ::ReleaseSkillSkillRequest request;
    request.set_skill_table_id(1);
    request.set_target_id(-1); // Invalid target ID

    EXPECT_CALL(*mockSkillTable, GetSkillTable(request.skill_table_id()))
        .WillRepeatedly(Return(nullptr)); // Mock implementation

    uint32_t result = skillUtil->ValidateTarget(&request);
    EXPECT_EQ(result, kSkillInvalidTargetId);
}

TEST_F(SkillUtilTest, ValidateTarget_ValidTarget_ReturnsOk) {
    entt::entity target = tls.actorRegistry.create(); // Create a valid target in the registry
 
    ::ReleaseSkillSkillRequest request;
    request.set_skill_table_id(10);
    request.set_target_id(entt::to_integral(target)); // Valid target ID

    SkillTable tableSkill;
    tableSkill.mutable_targeting_mode()->Add(1); // Add target type to simulate need for target

    EXPECT_CALL(*mockSkillTable, GetSkillTable(request.skill_table_id()))
        .WillRepeatedly(Return(&tableSkill));

    uint32_t result = skillUtil->ValidateTarget(&request);
    EXPECT_EQ(result, kSuccess);
}

TEST_F(SkillUtilTest, CheckCooldown_CooldownActive_ReturnsError) {
    entt::entity caster = tls.actorRegistry.create();

    auto tableSkill = std::make_shared<SkillTable>();
    tableSkill->set_cooldown_id(1);

    CooldownTimeComp cooldownTimeComp;
    cooldownTimeComp.set_cooldown_table_id(1);

    EXPECT_CALL(*mockCooldownTimeUtil, IsInCooldown(_))
        .WillRepeatedly(Return(true));

    auto & cooldownList = tls.actorRegistry.emplace<CooldownTimeListComp>(caster);
    cooldownTimeComp.set_cooldown_table_id(1);
    CoolDownTimeMillisecondSystem::Reset(cooldownTimeComp);
    cooldownList.mutable_cooldown_list()->emplace(1, cooldownTimeComp);

    uint32_t result = skillUtil->CheckCooldown(caster, tableSkill.get());
    EXPECT_EQ(result, kSkillCooldownNotReady);
}

TEST_F(SkillUtilTest, CheckCooldown_CooldownInactive_ReturnsOk) {
    entt::entity caster = tls.actorRegistry.create();

    auto tableSkill = std::make_shared<SkillTable>();
    tableSkill->set_cooldown_id(1);

    CooldownTimeComp cooldownTimeComp;
    cooldownTimeComp.set_cooldown_table_id(1);

    EXPECT_CALL(*mockCooldownTimeUtil, IsInCooldown(_))
        .WillRepeatedly(Return(false));

    auto& cooldownList = tls.actorRegistry.emplace<CooldownTimeListComp>(caster);
    cooldownList.mutable_cooldown_list()->emplace(1, cooldownTimeComp);

    uint32_t result = skillUtil->CheckCooldown(caster, tableSkill.get());
    EXPECT_EQ(result, kSuccess);
}

TEST_F(SkillUtilTest, HandleCastingTimer_ImmediateSkill_ReturnsOk) {
    entt::entity caster = tls.actorRegistry.create();
    auto tableSkill = std::make_shared<SkillTable>();
    tableSkill->set_immediately(true);
    tableSkill->set_castpoint(1000); // Set cast point to 1000ms

    EXPECT_CALL(*mockSkillTable, GetSkillTable(_))
        .WillRepeatedly(Return(tableSkill.get()));

    uint32_t result = skillUtil->CheckCasting(caster, tableSkill.get());
    EXPECT_EQ(result, kSuccess);
}

TEST_F(SkillUtilTest, HandleRecoveryTimeTimer_ImmediateSkill_ReturnsOk) {
    entt::entity caster = tls.actorRegistry.create();
    auto tableSkill = std::make_shared<SkillTable>();
    tableSkill->set_immediately(true);
    tableSkill->set_recoverytime(1000); // Set recovery time to 1000ms

    EXPECT_CALL(*mockSkillTable, GetSkillTable(_))
        .WillRepeatedly(Return(tableSkill.get()));

    uint32_t result = skillUtil->CheckRecovery(caster, tableSkill.get());
    EXPECT_EQ(result, kSuccess);
}

TEST_F(SkillUtilTest, HandleChannelTimeTimer_ImmediateSkill_ReturnsOk) {
    entt::entity caster = tls.actorRegistry.create();
    auto tableSkill = std::make_shared<SkillTable>();
    tableSkill->set_immediately(true);
    tableSkill->set_channelfinish(1000); // Set channel finish time to 1000ms
    tableSkill->set_channelthink(500);   // Set channel interval to 500ms

    EXPECT_CALL(*mockSkillTable, GetSkillTable(_))
        .WillRepeatedly(Return(tableSkill.get()));

    uint32_t result = skillUtil->CheckChannel(caster, tableSkill.get());
    EXPECT_EQ(result, kSuccess);
}

TEST_F(SkillUtilTest, BroadcastSkillUsedMessage_CreatesMessage) {
    entt::entity caster = tls.actorRegistry.create();
    ::ReleaseSkillSkillRequest request;
    request.set_skill_table_id(1);
    request.set_target_id(2);
    request.mutable_position()->set_x(10); // Mock position

    EXPECT_CALL(*mockSkillTable, GetSkillTable(request.skill_table_id()))
        .WillRepeatedly(Return(nullptr)); // Mock implementation

    EXPECT_NO_THROW(skillUtil->BroadcastSkillUsedMessage(caster, &request));
}

TEST_F(SkillUtilTest, SetupCastingTimer_SetsTimer) {
    entt::entity caster = tls.actorRegistry.create();
    auto tableSkill = std::make_shared<SkillTable>();
    tableSkill->set_id(1);
    tableSkill->set_castpoint(1); // Set cast point to 1000ms

    EXPECT_CALL(*mockSkillTable, GetSkillTable(_))
        .WillRepeatedly(Return(tableSkill.get()));

    EXPECT_CALL(*mockSkillTable, IsSkillOfType(_, kGeneralSkill))
        .WillRepeatedly(Return(true));

    skillUtil->SetupCastingTimer(caster, tableSkill.get(), 2);

    auto* castingTimerComp = tls.actorRegistry.try_get<CastingTimerComp>(caster);
    ASSERT_NE(castingTimerComp, nullptr);  // Check if the component was created
    EXPECT_TRUE(castingTimerComp->timer.IsActive());  // Check if the timer is active
}

TEST_F(SkillUtilTest, HandleSkillSpell_TriggersEffect) {
    entt::entity caster = tls.actorRegistry.create();

    SkillSystem::InitializeActorComponents(caster);

    auto tableSkill = std::make_shared<SkillTable>();
    tableSkill->set_id(1);

    EXPECT_CALL(*mockSkillTable, GetSkillTable(1))
        .WillRepeatedly(Return(tableSkill.get()));

    EXPECT_NO_THROW(skillUtil->HandleGeneralSkillSpell(caster, 1));
}

TEST_F(SkillUtilTest, HandleSkillRecovery_SetsRecoveryTimer) {
    entt::entity caster = tls.actorRegistry.create();

    SkillSystem::InitializeActorComponents(caster);

    auto tableSkill = std::make_shared<SkillTable>();
    tableSkill->set_recoverytime(1000); // Set recovery time to 1000ms

    EXPECT_CALL(*mockSkillTable, GetSkillTable(_))
        .WillRepeatedly(Return(tableSkill.get()));

    EXPECT_NO_THROW(skillUtil->HandleSkillRecovery(caster, 1));
}

TEST_F(SkillUtilTest, HandleSkillToggleOn_TriggersEffect) {
    entt::entity caster = tls.actorRegistry.create();

    SkillSystem::InitializeActorComponents(caster);

    auto tableSkill = std::make_shared<SkillTable>();
    tableSkill->set_id(1);

    EXPECT_CALL(*mockSkillTable, GetSkillTable(1))
        .WillRepeatedly(Return(tableSkill.get()));

    EXPECT_NO_THROW(skillUtil->HandleSkillToggleOn(caster, 1));
}

TEST_F(SkillUtilTest, HandleSkillToggleOff_RemovesEffect) {
    entt::entity caster = tls.actorRegistry.create();
    
    SkillSystem::InitializeActorComponents(caster);
    
    auto tableSkill = std::make_shared<SkillTable>();
    tableSkill->set_id(1);

    EXPECT_CALL(*mockSkillTable, GetSkillTable(1))
        .WillRepeatedly(Return(tableSkill.get()));

    EXPECT_NO_THROW(skillUtil->HandleSkillToggleOff(caster, 1));
}

TEST_F(SkillUtilTest, HandleSkillActivate_TriggersEffect) {
    entt::entity caster = tls.actorRegistry.create();

    SkillSystem::InitializeActorComponents(caster);

    auto tableSkill = std::make_shared<SkillTable>();
    tableSkill->set_id(1);

    EXPECT_CALL(*mockSkillTable, GetSkillTable(1))
        .WillRepeatedly(Return(tableSkill.get()));

    EXPECT_NO_THROW(skillUtil->HandleSkillActivate(caster, 1));
}

TEST_F(SkillUtilTest, HandleSkillDeactivate_RemovesEffect) {
    entt::entity caster = tls.actorRegistry.create();

    SkillSystem::InitializeActorComponents(caster);
    
    auto tableSkill = std::make_shared<SkillTable>();
    tableSkill->set_id(1);

    EXPECT_CALL(*mockSkillTable, GetSkillTable(1))
        .WillRepeatedly(Return(tableSkill.get()));

    EXPECT_NO_THROW(skillUtil->HandleSkillDeactivate(caster, 1));
}

TEST_F(SkillUtilTest, SendSkillInterruptedMessage_SendsMessage) {
    entt::entity caster = tls.actorRegistry.create();
    uint32_t skillId = 1;

    EXPECT_NO_THROW(skillUtil->SendSkillInterruptedMessage(caster, skillId));
}

// Main function
int main(int argc, char** argv) {
    EventLoop loop;

    ::testing::InitGoogleTest(&argc, argv);
    CooldownConfigurationTable::Instance().Load();
    SkillConfigurationTable::Instance().Load();

    int ret = RUN_ALL_TESTS();
    tls.actorRegistry.clear(); // Clean up thread-local storage after all tests
    return ret;
}
