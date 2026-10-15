// BuffSystem 周期回调期间改写 buff 列表的回归用例。
//
// BuffSystem::OnIntervalThink 依次调三路 impl 的周期回调。第一路 BuffImplSystem 会顺着
// TickCombatIdleBuff -> AddSubBuffs -> AddOrUpdateBuff 往同一张 BuffListComp(unordered_map)
// 里插子 buff(可能扩容),子 buff 的 DispelBuffsOnAwake 还可能把发起 tick 的这条 buff 自己驱散掉。
// 旧写法在三路之间一直端着同一个 buffIt:扩容后迭代器失效,驱散后解引用的是已释放节点。
//
// 配表用夹具自带的 buff.json(写进临时目录再 Load),不依赖 bin/ 下的真实配表 ——
// 真实配表里目前没有能触发这条路径的组合(PROGRESS 2026-10-14 四路检查 P2)。

#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <system_error>

#include "combat/buff/comp/buff_comp.h"
#include "combat/buff/system/buff.h"
#include "proto/common/component/buff_comp.pb.h"
#include "table/code/buff_table.h"
#include "table/proto/tip/common_error_tip.pb.h"
#include "thread_context/ecs_context.h"
#include "time/system/time.h"
#include <node_config_manager.h>

namespace
{
namespace fs = std::filesystem;

// 夹具配表 id 段,与真实配表无关(测试进程只加载下面这份 buff.json)。
constexpr uint32_t kOwnerDispelledBySub = 9001;  // 周期 tick 加子 buff,子 buff 反手驱散它
constexpr uint32_t kDispelCombatIdle = 9002;     // 纯驱散 buff,驱散带 combat_idle 标签的 buff
constexpr uint32_t kOwnerWithManySubs = 9003;    // 周期 tick 一次加 16 个子 buff,撑爆桶数
constexpr uint32_t kPlainSubBuff = 9004;         // 无任何回调副作用的普通子 buff
constexpr int kManySubBuffCount = 16;

// buff_type 一律显式写:缺省 0 是 kBuffTypeMovementSpeedReduction,会顺带去标记属性重算。
// duration 取 -1(永久):正数会挂到期定时器,测试进程里没有 EventLoop。
// combat_idle_seconds 取极大值,让 TickCombatIdleBuff 必然走到 AddSubBuffs 分支,不依赖墙钟快慢。
std::string FixtureBuffTableJson()
{
	std::string manySubs;
	for (int i = 0; i < kManySubBuffCount; ++i)
	{
		manySubs += (i == 0 ? "" : ",") + std::to_string(kPlainSubBuff);
	}

	return R"({"data":[)"
		R"({"id":9001,"buff_type":43,"duration":-1,"interval":1,"combat_idle_seconds":1e15,)"
		R"("tag":{"combat_idle":true},"sub_buff":[9002]},)"
		R"({"id":9002,"buff_type":35,"duration":-1,"dispel_tag":{"combat_idle":true}},)"
		R"({"id":9003,"buff_type":43,"duration":-1,"interval":1,"combat_idle_seconds":1e15,)"
		R"("sub_buff":[)" + manySubs + R"(]},)"
		R"({"id":9004,"buff_type":10,"duration":-1})"
		R"(]})";
}

// 让 TickCombatIdleBuff 认为"刚被打过",从而这一 tick 去加子 buff。
void MarkCombatIdleJustReset(entt::entity actor, uint64_t buffId)
{
	auto &entry = tlsEcs.actorRegistry.get<BuffListComp>(actor).at(buffId);
	const auto data = std::dynamic_pointer_cast<BuffNoDamageOrSkillHitInLastSecondsComp>(entry.dataPbPtr);
	ASSERT_NE(nullptr, data);
	data->set_last_time(TimeSystem::NowMilliseconds());
}

uint64_t AddBuffOrFail(entt::entity actor, uint32_t buffTableId)
{
	const auto [result, buffId] = BuffSystem::AddOrUpdateBuff(actor, buffTableId);
	EXPECT_EQ(kSuccess, result);
	EXPECT_NE(UINT64_MAX, buffId);
	return buffId;
}
} // namespace

class BuffIntervalThinkTest : public ::testing::Test
{
protected:
	static void SetUpTestSuite()
	{
		tableDir = fs::temp_directory_path() / "mmorpg_buff_test_tables";
		std::error_code ec;
		fs::create_directories(tableDir, ec);
		ASSERT_FALSE(ec) << ec.message();
		{
			std::ofstream out(tableDir / "buff.json", std::ios::binary | std::ios::trunc);
			ASSERT_TRUE(out.is_open());
			out << FixtureBuffTableJson();
		}

		auto &deployConfig = tlsNodeConfigManager.GetBaseDeployConfig();
		deployConfig.set_table_data_format("json");
		deployConfig.set_table_data_directory((tableDir / "").string());
		BuffTableManager::Instance().Load();
		ASSERT_TRUE(BuffTableManager::Instance().Exists(kOwnerDispelledBySub));
		ASSERT_TRUE(BuffTableManager::Instance().Exists(kOwnerWithManySubs));
	}

	static void TearDownTestSuite()
	{
		std::error_code ec;
		fs::remove_all(tableDir, ec);
	}

	void TearDown() override
	{
		tlsEcs.actorRegistry.clear();
	}

	static inline fs::path tableDir;
};

// 子 buff 在第一路回调里把发起 tick 的 buff 驱散掉:后两路回调必须停手,
// 不能再拿旧迭代器去解引用已 erase 的节点(Debug 下 MSVC 的迭代器检查会直接断言)。
TEST_F(BuffIntervalThinkTest, SubBuffDispelsOwner_StopsBeforeRemainingHandlers)
{
	const auto actor = tlsEcs.actorRegistry.create();
	const auto ownerId = AddBuffOrFail(actor, kOwnerDispelledBySub);
	MarkCombatIdleJustReset(actor, ownerId);

	BuffSystem::OnIntervalThink(actor, ownerId);

	const auto &buffList = tlsEcs.actorRegistry.get<BuffListComp>(actor);
	EXPECT_FALSE(buffList.contains(ownerId));
	EXPECT_TRUE(buffList.empty()) << "纯驱散子 buff 不应留在列表里";
}

// 同一场景走每帧入口:UpdatePeriodicBuff 在回调后重查,owner 没了就不再写回计时器。
TEST_F(BuffIntervalThinkTest, SubBuffDispelsOwner_ViaFrameUpdate)
{
	const auto actor = tlsEcs.actorRegistry.create();
	const auto ownerId = AddBuffOrFail(actor, kOwnerDispelledBySub);
	MarkCombatIdleJustReset(actor, ownerId);

	BuffSystem::Update(1.0);  // == interval,恰好 tick 一次

	EXPECT_TRUE(tlsEcs.actorRegistry.get<BuffListComp>(actor).empty());
}

// 子 buff 插入让 unordered_map 扩容:owner 仍在,且 16 个子 buff 全部登记进 owner 的
// sub_buff_list_id、全部真实存在于列表中。
//
// MSVC 的 unordered_map 扩容不会让节点迭代器失效,所以旧写法在本平台上这条用例也是绿的;
// 它守的是"回调中途扩容"这一场景的可观察结果,真正红/绿取决于上面的驱散用例。
TEST_F(BuffIntervalThinkTest, SubBuffsRehashList_OwnerRecordsEverySubBuff)
{
	const auto actor = tlsEcs.actorRegistry.create();
	const auto ownerId = AddBuffOrFail(actor, kOwnerWithManySubs);
	MarkCombatIdleJustReset(actor, ownerId);
	const auto bucketsBefore = tlsEcs.actorRegistry.get<BuffListComp>(actor).bucket_count();

	BuffSystem::OnIntervalThink(actor, ownerId);

	const auto &buffList = tlsEcs.actorRegistry.get<BuffListComp>(actor);
	ASSERT_GT(buffList.bucket_count(), bucketsBefore) << "夹具没能触发扩容,用例失去意义";
	ASSERT_TRUE(buffList.contains(ownerId));

	const auto &owner = buffList.at(ownerId).buffPb;
	EXPECT_TRUE(owner.has_added_sub_buff());
	EXPECT_EQ(static_cast<int>(kManySubBuffCount), owner.sub_buff_list_id_size());
	EXPECT_EQ(static_cast<std::size_t>(kManySubBuffCount + 1), buffList.size());
	for (const auto &[subBuffId, activated] : owner.sub_buff_list_id())
	{
		EXPECT_TRUE(buffList.contains(subBuffId)) << "sub buff " << subBuffId;
		EXPECT_FALSE(activated);
	}
}

int main(int argc, char **argv)
{
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
