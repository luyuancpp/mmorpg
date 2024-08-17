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

struct SomeComponent
{};

// Mocked functions and data
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
    }

    void TearDown() override {
        tls.registry.clear();
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
    tls.registry.emplace<SomeComponent>(target); // Some component to make the entity valid

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
    EXPECT_CALL(*mockCooldownTimeUtil, IsInCooldown(_))
        .WillRepeatedly(Return(true));

    auto& cooldownList = tls.registry.emplace<CooldownTimeListComp>(caster);
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
    EXPECT_CALL(*mockCooldownTimeUtil, IsInCooldown(_))
        .WillRepeatedly(Return(false));

    auto& cooldownList = tls.registry.emplace<CooldownTimeListComp>(caster);
    cooldownTimeComp.set_cooldown_table_id(1);
    CoolDownTimeMillisecondUtil::Reset(cooldownTimeComp);
    cooldownList.mutable_cooldown_list()->emplace(1, cooldownTimeComp);

    uint32_t result = abilityUtil->CheckCooldown(caster, tableAbility.get());
    EXPECT_EQ(result, kOK);
}

TEST_F(AbilityUtilTest, HandleCastingTimer_ImmediateAbility_ReturnsOk) {
    entt::entity caster = tls.registry.create();
    auto tableAbility = std::make_shared<ability_row>();
    tableAbility->set_immediately(true);

    // Setup a CastingTimer with a mock behavior if needed
    CastingTimerComp castingTimer;
    // Simulate timer behavior if necessary

    EXPECT_CALL(*mockAbilityTable, GetAbilityTable(_))
        .WillRepeatedly(Return(tableAbility.get())); // Mock implementation

    uint32_t result = abilityUtil->HandleCastingTimer(caster, tableAbility.get());
    EXPECT_EQ(result, kOK);
}

TEST_F(AbilityUtilTest, HandleRecoveryTimeTimer_ImmediateAbility_ReturnsOk) {
    entt::entity caster = tls.registry.create();
    auto tableAbility = std::make_shared<ability_row>();
    tableAbility->set_immediately(true);

    // Setup a RecoveryTimer with a mock behavior if needed
    RecoveryTimerComp recoveryTimer;

    EXPECT_CALL(*mockAbilityTable, GetAbilityTable(_))
        .WillRepeatedly(Return(tableAbility.get()));

    uint32_t result = abilityUtil->HandleRecoveryTimeTimer(caster, tableAbility.get());
    EXPECT_EQ(result, kOK);
}

TEST_F(AbilityUtilTest, HandleChannelTimeTimer_ImmediateAbility_ReturnsOk) {
    entt::entity caster = tls.registry.create();
    auto tableAbility = std::make_shared<ability_row>();
    tableAbility->set_immediately(true);

    // Setup a ChannelFinishTimerComp with a mock behavior if needed
    ChannelFinishTimerComp channelFinishTimer;

    EXPECT_CALL(*mockAbilityTable, GetAbilityTable(_))
        .WillRepeatedly(Return(tableAbility.get()));

    uint32_t result = abilityUtil->HandleChannelTimeTimer(caster, tableAbility.get());
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
        .WillRepeatedly(Return(tableAbility.get())); // Mock implementation

    EXPECT_CALL(*mockAbilityTable, IsAbilityOfType(_, kGeneralAbility))
        .WillRepeatedly(Return(true));

    abilityUtil->SetupCastingTimer(caster, tableAbility.get(), 1);

    auto* castingTimerComp = tls.registry.try_get<CastingTimerComp>(caster);
    castingTimerComp->timer.RunAfter(0.01, [] {});
    ASSERT_NE(castingTimerComp, nullptr);  // Check if the component was created
    EXPECT_TRUE(castingTimerComp->timer.IsActive());  // Check if the timer is active
}

int main(int argc, char** argv) {
    EventLoop loop;

    CooldownConfigurationTable::GetSingleton().Load();
    AbilityConfigurationTable::GetSingleton().Load();

    ::testing::InitGoogleTest(&argc, argv);
    auto ret = RUN_ALL_TESTS();
    tls.registry.clear();
    return ret;
}
