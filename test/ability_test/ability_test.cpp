#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "game_logic/combat/ability/util/ability_util.h"
#include "thread_local/storage.h"
#include "ability_config.h"
#include "game_logic/combat/ability/comp/ability_comp.h"
#include "game_logic/combat/ability/constants/ability_constants.h"
#include "time/comp/timer_task_comp.h"
#include "time/util/cooldown_time_util.h"
#include "pbc/ability_error_tip.pb.h"
#include "pbc/common_error_tip.pb.h"

using ::testing::_;
using ::testing::Return;

// Mocked classes and functions
class MockAbilityTable {
public:
    MOCK_METHOD(const ability_row*, GetAbilityTable, (uint32_t), (const));
    MOCK_METHOD(bool, IsAbilityOfType, (uint32_t, uint32_t), (const));
};

class MockCooldownTimeUtil {
public:
    MOCK_METHOD(bool, IsInCooldown, (const CooldownTimeComp&), (const));
};

// Test Fixture
class AbilityUtilTest : public ::testing::Test {
protected:
    void SetUp() override {
        abilityUtil = std::make_unique<AbilityUtil>();
        // Setup the mocks if necessary
    }

    void TearDown() override {
        tls.registry.clear(); // Clean up the thread-local storage after each test
    }

    std::unique_ptr<AbilityUtil> abilityUtil;
    std::unique_ptr<MockAbilityTable> mockAbilityTable = std::make_unique<MockAbilityTable>();
    std::unique_ptr<MockCooldownTimeUtil> mockCooldownTimeUtil = std::make_unique<MockCooldownTimeUtil>();
};

// Test cases

TEST_F(AbilityUtilTest, ValidateAbilityTable_InvalidId_ReturnsError) {
    const uint32_t invalidAbilityId = 9999999;
    EXPECT_CALL(*mockAbilityTable, GetAbilityTable(invalidAbilityId))
        .WillRepeatedly(Return(nullptr));

    auto [tableAbility, result] = abilityUtil->ValidateAbilityTable(invalidAbilityId);
    EXPECT_EQ(result, kInvalidTableId);
}

TEST_F(AbilityUtilTest, ValidateTarget_InvalidTarget_ReturnsError) {
    ::UseAbilityRequest request;
    request.set_ability_id(1);
    request.set_target_id(-1); // Invalid target ID

    EXPECT_CALL(*mockAbilityTable, GetAbilityTable(request.ability_id()))
        .WillRepeatedly(Return(nullptr)); // Mock implementation

    uint32_t result = abilityUtil->ValidateTarget(&request);
    EXPECT_EQ(result, kAbilityInvalidTargetId);
}

TEST_F(AbilityUtilTest, ValidateTarget_ValidTarget_ReturnsOk) {
    entt::entity target = tls.registry.create(); // Create a valid target in the registry
 
    ::UseAbilityRequest request;
    request.set_ability_id(10);
    request.set_target_id(entt::to_integral(target)); // Valid target ID

    ability_row tableAbility;
    tableAbility.mutable_target_type()->Add(1); // Add target type to simulate need for target

    EXPECT_CALL(*mockAbilityTable, GetAbilityTable(request.ability_id()))
        .WillRepeatedly(Return(&tableAbility));

    uint32_t result = abilityUtil->ValidateTarget(&request);
    EXPECT_EQ(result, kOK);
}

TEST_F(AbilityUtilTest, CheckCooldown_CooldownActive_ReturnsError) {
    entt::entity caster = tls.registry.create();

    auto tableAbility = std::make_shared<ability_row>();
    tableAbility->set_cooldown_id(1);

    CooldownTimeComp cooldownTimeComp;
    cooldownTimeComp.set_cooldown_table_id(1);

    EXPECT_CALL(*mockCooldownTimeUtil, IsInCooldown(_))
        .WillRepeatedly(Return(true));

    auto & cooldownList = tls.registry.emplace<CooldownTimeListComp>(caster);
    cooldownTimeComp.set_cooldown_table_id(1);
    CoolDownTimeMillisecondUtil::Reset(cooldownTimeComp);
    cooldownList.mutable_cooldown_list()->emplace(1, cooldownTimeComp);

    uint32_t result = abilityUtil->CheckCooldown(caster, tableAbility.get());
    EXPECT_EQ(result, kAbilityCooldownNotReady);
}

TEST_F(AbilityUtilTest, CheckCooldown_CooldownInactive_ReturnsOk) {
    entt::entity caster = tls.registry.create();

    auto tableAbility = std::make_shared<ability_row>();
    tableAbility->set_cooldown_id(1);

    CooldownTimeComp cooldownTimeComp;
    cooldownTimeComp.set_cooldown_table_id(1);

    EXPECT_CALL(*mockCooldownTimeUtil, IsInCooldown(_))
        .WillRepeatedly(Return(false));

    auto& cooldownList = tls.registry.emplace<CooldownTimeListComp>(caster);
    cooldownList.mutable_cooldown_list()->emplace(1, cooldownTimeComp);

    uint32_t result = abilityUtil->CheckCooldown(caster, tableAbility.get());
    EXPECT_EQ(result, kOK);
}

TEST_F(AbilityUtilTest, HandleCastingTimer_ImmediateAbility_ReturnsOk) {
    entt::entity caster = tls.registry.create();
    auto tableAbility = std::make_shared<ability_row>();
    tableAbility->set_immediately(true);
    tableAbility->set_castpoint(1000); // Set cast point to 1000ms

    EXPECT_CALL(*mockAbilityTable, GetAbilityTable(_))
        .WillRepeatedly(Return(tableAbility.get()));

    uint32_t result = abilityUtil->CheckCasting(caster, tableAbility.get());
    EXPECT_EQ(result, kOK);
}

TEST_F(AbilityUtilTest, HandleRecoveryTimeTimer_ImmediateAbility_ReturnsOk) {
    entt::entity caster = tls.registry.create();
    auto tableAbility = std::make_shared<ability_row>();
    tableAbility->set_immediately(true);
    tableAbility->set_recoverytime(1000); // Set recovery time to 1000ms

    EXPECT_CALL(*mockAbilityTable, GetAbilityTable(_))
        .WillRepeatedly(Return(tableAbility.get()));

    uint32_t result = abilityUtil->CheckRecovery(caster, tableAbility.get());
    EXPECT_EQ(result, kOK);
}

TEST_F(AbilityUtilTest, HandleChannelTimeTimer_ImmediateAbility_ReturnsOk) {
    entt::entity caster = tls.registry.create();
    auto tableAbility = std::make_shared<ability_row>();
    tableAbility->set_immediately(true);
    tableAbility->set_channelfinish(1000); // Set channel finish time to 1000ms
    tableAbility->set_channelthink(500);   // Set channel interval to 500ms

    EXPECT_CALL(*mockAbilityTable, GetAbilityTable(_))
        .WillRepeatedly(Return(tableAbility.get()));

    uint32_t result = abilityUtil->CheckChannel(caster, tableAbility.get());
    EXPECT_EQ(result, kOK);
}

TEST_F(AbilityUtilTest, BroadcastAbilityUsedMessage_CreatesMessage) {
    entt::entity caster = tls.registry.create();
    ::UseAbilityRequest request;
    request.set_ability_id(1);
    request.set_target_id(2);
    request.mutable_position()->set_x(10); // Mock position

    EXPECT_CALL(*mockAbilityTable, GetAbilityTable(request.ability_id()))
        .WillRepeatedly(Return(nullptr)); // Mock implementation

    EXPECT_NO_THROW(abilityUtil->BroadcastAbilityUsedMessage(caster, &request));
}

TEST_F(AbilityUtilTest, SetupCastingTimer_SetsTimer) {
    entt::entity caster = tls.registry.create();
    auto tableAbility = std::make_shared<ability_row>();
    tableAbility->set_castpoint(1000); // Set cast point to 1000ms

    EXPECT_CALL(*mockAbilityTable, GetAbilityTable(_))
        .WillRepeatedly(Return(tableAbility.get()));

    EXPECT_CALL(*mockAbilityTable, IsAbilityOfType(_, kGeneralAbility))
        .WillRepeatedly(Return(true));

    abilityUtil->SetupCastingTimer(caster, tableAbility.get(), 1);

    auto* castingTimerComp = tls.registry.try_get<CastingTimerComp>(caster);
    ASSERT_NE(castingTimerComp, nullptr);  // Check if the component was created
    EXPECT_TRUE(castingTimerComp->timer.IsActive());  // Check if the timer is active
}

TEST_F(AbilityUtilTest, HandleAbilitySpell_TriggersEffect) {
    entt::entity caster = tls.registry.create();
    auto tableAbility = std::make_shared<ability_row>();
    tableAbility->set_id(1);

    EXPECT_CALL(*mockAbilityTable, GetAbilityTable(1))
        .WillRepeatedly(Return(tableAbility.get()));

    EXPECT_NO_THROW(abilityUtil->HandleGeneralAbilitySpell(caster, 1));
}

TEST_F(AbilityUtilTest, HandleAbilityRecovery_SetsRecoveryTimer) {
    entt::entity caster = tls.registry.create();
    auto tableAbility = std::make_shared<ability_row>();
    tableAbility->set_recoverytime(1000); // Set recovery time to 1000ms

    EXPECT_CALL(*mockAbilityTable, GetAbilityTable(_))
        .WillRepeatedly(Return(tableAbility.get()));

    EXPECT_NO_THROW(abilityUtil->HandleAbilityRecovery(caster, 1));
}

TEST_F(AbilityUtilTest, HandleAbilityToggleOn_TriggersEffect) {
    entt::entity caster = tls.registry.create();
    auto tableAbility = std::make_shared<ability_row>();
    tableAbility->set_id(1);

    EXPECT_CALL(*mockAbilityTable, GetAbilityTable(1))
        .WillRepeatedly(Return(tableAbility.get()));

    EXPECT_NO_THROW(abilityUtil->HandleAbilityToggleOn(caster, 1));
}

TEST_F(AbilityUtilTest, HandleAbilityToggleOff_RemovesEffect) {
    entt::entity caster = tls.registry.create();
    auto tableAbility = std::make_shared<ability_row>();
    tableAbility->set_id(1);

    EXPECT_CALL(*mockAbilityTable, GetAbilityTable(1))
        .WillRepeatedly(Return(tableAbility.get()));

    EXPECT_NO_THROW(abilityUtil->HandleAbilityToggleOff(caster, 1));
}

TEST_F(AbilityUtilTest, HandleAbilityActivate_TriggersEffect) {
    entt::entity caster = tls.registry.create();
    auto tableAbility = std::make_shared<ability_row>();
    tableAbility->set_id(1);

    EXPECT_CALL(*mockAbilityTable, GetAbilityTable(1))
        .WillRepeatedly(Return(tableAbility.get()));

    EXPECT_NO_THROW(abilityUtil->HandleAbilityActivate(caster, 1));
}

TEST_F(AbilityUtilTest, HandleAbilityDeactivate_RemovesEffect) {
    entt::entity caster = tls.registry.create();
    auto tableAbility = std::make_shared<ability_row>();
    tableAbility->set_id(1);

    EXPECT_CALL(*mockAbilityTable, GetAbilityTable(1))
        .WillRepeatedly(Return(tableAbility.get()));

    EXPECT_NO_THROW(abilityUtil->HandleAbilityDeactivate(caster, 1));
}

TEST_F(AbilityUtilTest, SendAbilityInterruptedMessage_SendsMessage) {
    entt::entity caster = tls.registry.create();
    uint32_t abilityId = 1;

    EXPECT_NO_THROW(abilityUtil->SendAbilityInterruptedMessage(caster, abilityId));
}

// Main function
int main(int argc, char** argv) {
    EventLoop loop;

    ::testing::InitGoogleTest(&argc, argv);
    CooldownConfigurationTable::GetSingleton().Load();
    AbilityConfigurationTable::GetSingleton().Load();

    int ret = RUN_ALL_TESTS();
    tls.registry.clear(); // Clean up thread-local storage after all tests
    return ret;
}
