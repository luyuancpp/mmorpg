#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "game_logic/combat/buff/util/buff_util.h"
#include "buff_config.h"
#include "buff_error_tip.pb.h"
#include "thread_local/storage.h"
#include "proto/logic/event/buff_event.pb.h"
#include "game_logic/combat/buff/comp/buff_comp.h"
#include "time/comp/timer_task_comp.h"
#include "pbc/buff_error_tip.pb.h"
#include "pbc/common_error_tip.pb.h"



class BuffUtilTest : public ::testing::Test {
protected:
	void SetUp() override {
		// Setup code here, if needed
		tls.registry.clear(); // 清空线程局部存储
	}

	void TearDown() override {
		// Cleanup code here, if needed
		tls.registry.clear(); // 清空线程局部存储
	}
};

TEST_F(BuffUtilTest, AddOrUpdateBuffSuccess) {
	uint32_t buffTableId = 1;
	entt::entity parent = tls.registry.create();
	auto abilityContext = std::make_shared<SkillContextPBComponent>();

	// Mock BuffTable
	BuffTable mockBuffTable;
	mockBuffTable.set_nocaster(false); // 设置一些默认值
	mockBuffTable.set_maxlayer(3);

	// Add a BuffTable to the registry
	// Assuming you have a way to add a mock BuffTable to your system
	// AddBuffTableToRegistry(buffTableId, mockBuffTable);

	// Set up a BuffListComp for the parent entity
	BuffListComp& buffListComp = tls.registry.emplace<BuffListComp>(parent);
	buffListComp.buffList.clear(); // Ensure it's empty for this test

	// Call the AddOrUpdateBuff method
	uint32_t result = BuffUtil::AddOrUpdateBuff(parent, buffTableId, abilityContext);

	// Verify the result
	EXPECT_EQ(result, kOK);

	// Verify that the Buff was added to the BuffListComp
	const auto& buffList = tls.registry.get<BuffListComp>(parent).buffList;
	EXPECT_FALSE(buffList.empty());
}

TEST_F(BuffUtilTest, CanCreateBuffSuccess) {
	uint32_t buffTableId = 1;
	entt::entity parent = tls.registry.create();

	// Mock BuffTable
	BuffTable mockBuffTable;
	mockBuffTable.mutable_immunetag()->emplace( "immunetag1", true );

	// Add a BuffTable to the registry
	// AddBuffTableToRegistry(buffTableId, mockBuffTable);

	// Set up a BuffListComp for the parent entity
	BuffListComp& buffListComp = tls.registry.emplace<BuffListComp>(parent);
	buffListComp.buffList.clear(); // Ensure it's empty for this test

	// Call the CanCreateBuff method
	uint32_t result = BuffUtil::CanCreateBuff(parent, buffTableId);

	// Verify the result
	EXPECT_EQ(result, kOK);
}

// 你可以添加更多的测试用例来验证其他静态方法的行为

int main(int argc, char** argv) {
	EventLoop loop;

	::testing::InitGoogleTest(&argc, argv);
	BuffConfigurationTable::GetSingleton().Load();
	int ret = RUN_ALL_TESTS();
	tls.registry.clear(); // Clean up thread-local storage after all tests
	return ret;
}
