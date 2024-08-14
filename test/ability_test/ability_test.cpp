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
		mockAbilityTable = std::make_unique<MockAbilityTable>();
		mockCooldownTimeUtil = std::make_unique<MockCooldownTimeUtil>();

		// Optionally, you can use dependency injection or setup to replace real implementations
	}

	std::unique_ptr<AbilityUtil> abilityUtil;
	std::unique_ptr<MockAbilityTable> mockAbilityTable;
	std::unique_ptr<MockCooldownTimeUtil> mockCooldownTimeUtil;
};

TEST_F(AbilityUtilTest, ValidateAbilityTable_InvalidId_ReturnsError) {
	const uint32_t invalidAbilityId = 9999;
	EXPECT_CALL(*mockAbilityTable, GetAbilityTable(invalidAbilityId))
		.WillOnce(Return(nullptr));

	auto [tableAbility, result] = abilityUtil->ValidateAbilityTable(invalidAbilityId);
	EXPECT_EQ(result, kInvalidTableId);
}

TEST_F(AbilityUtilTest, ValidateTarget_InvalidTarget_ReturnsError) {
	::UseAbilityRequest request;
	request.set_ability_id(1);
	request.set_target_id(-1); // Invalid target ID

	EXPECT_CALL(*mockAbilityTable, GetAbilityTable(request.ability_id()))
		.WillOnce(Return(nullptr)); // Mock implementation

	uint32_t result = abilityUtil->ValidateTarget(&request);
	EXPECT_EQ(result, kAbilityInvalidTargetId);
}

TEST_F(AbilityUtilTest, CheckCooldown_CooldownActive_ReturnsError) {
	entt::entity caster{ 1 };
	auto tableAbility = std::make_shared<ability_row>();
	tableAbility->set_cooldown_id(1);

	CooldownTimeComp cooldownTimeComp;
	EXPECT_CALL(*mockCooldownTimeUtil, IsInCooldown(_))
		.WillOnce(Return(true));

	uint32_t result = abilityUtil->CheckCooldown(caster, tableAbility.get());
	EXPECT_EQ(result, kAbilityCooldownNotReady);
}

TEST_F(AbilityUtilTest, HandleCastingTimer_ImmediateAbility_ReturnsOk) {
	entt::entity caster{ 1 };
	auto tableAbility = std::make_shared<ability_row>();
	tableAbility->set_immediately(true);

	// Setup a CastingTimer with a mock behavior if needed
	CastingTimer castingTimer;
	// Simulate timer behavior if necessary

	EXPECT_CALL(*mockAbilityTable, GetAbilityTable(_))
		.WillOnce(Return(tableAbility.get())); // Mock implementation

	uint32_t result = abilityUtil->HandleCastingTimer(caster, tableAbility.get());
	EXPECT_EQ(result, kOK);
}

TEST_F(AbilityUtilTest, BroadcastAbilityUsedMessage_CreatesMessage) {
	entt::entity caster{ 1 };
	::UseAbilityRequest request;
	request.set_ability_id(1);
	request.set_target_id(2);
	request.mutable_position()->set_x(10); // Mock position

	EXPECT_CALL(*mockAbilityTable, GetAbilityTable(request.ability_id()))
		.WillOnce(Return(nullptr)); // Mock implementation

	// Mock the ViewUtil::BroadcastMessageToVisiblePlayers function if possible
	// If not possible, test if BroadcastAbilityUsedMessage behaves correctly

	EXPECT_NO_THROW(abilityUtil->BroadcastAbilityUsedMessage(caster, &request));
}

TEST_F(AbilityUtilTest, SetupCastingTimer_SetsTimer) {
	entt::entity caster{ 1 };
	auto tableAbility = std::make_shared<ability_row>();
	tableAbility->set_castpoint(1000); // Set cast point to 1000ms

	EXPECT_CALL(*mockAbilityTable, GetAbilityTable(_))
		.WillOnce(Return(tableAbility.get())); // Mock implementation

	EXPECT_CALL(*mockAbilityTable, IsAbilityOfType(_, kGeneralAbility))
		.WillOnce(Return(true));

	EXPECT_NO_THROW(abilityUtil->SetupCastingTimer(caster, tableAbility.get(), 1));
}

int main(int argc, char** argv) {
	CooldownConfigurationTable::GetSingleton().Load();
	AbilityConfigurationTable::GetSingleton().Load();
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
