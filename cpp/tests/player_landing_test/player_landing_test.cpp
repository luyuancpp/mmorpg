#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "player/system/player_landing.h"

// ---------------------------------------------------------------------------
// player_landing_test —— 落地恢复版本摘要的解析(PlayerLandingSystem::ParseSyncDigestFields)。
// 设计文档:docs/design/landing-recovery-sync-digest.md §4.2 第 4 步。
//
// 盯住的是客户端可见契约的两条底线:
//   1) 能解析的摘要逐字段如实转出(g → generation,已知域 → entries),未知字段跳过不报错
//      —— Go 侧先上新域、旧 scene 未升级时字段会多于枚举;
//   2) 任何畸形输入都返回 false,让调用方推 UNAVAILABLE(客户端全量拉)。把畸形摘要当真,
//      客户端会把没拉过的版本记进缓存,违反 L-2「缓存版本不领先于数据」。
// 只测纯函数:不连 Redis、不建 ECS 实体,也不链接 scene 库。
// ---------------------------------------------------------------------------

namespace
{

using Fields = std::vector<std::pair<std::string, std::string>>;

// 测试里按域查版本;找不到返回 false。entries 的顺序没有业务含义,断言不依赖下标。
bool FindVersion(const SyncDigestS2C &digest, SyncDomain domain, uint64_t &version)
{
    for (const auto &entry : digest.entries())
    {
        if (entry.domain() == domain)
        {
            version = entry.version();
            return true;
        }
    }
    return false;
}

} // namespace

// ---- 正常 ----

TEST(ParseSyncDigestFields, ParsesGenerationAndKnownDomains)
{
    const Fields fields{
        {"g", "1760400000123"},
        {"1", "3"},
        {"4", "17"},
        {"9", "0"},
    };
    SyncDigestS2C digest;
    ASSERT_TRUE(PlayerLandingSystem::ParseSyncDigestFields(fields, &digest));

    EXPECT_EQ(digest.status(), SYNC_DIGEST_STATUS_OK);
    EXPECT_EQ(digest.generation(), 1760400000123ULL);
    ASSERT_EQ(digest.entries_size(), 3);

    uint64_t version = 0;
    ASSERT_TRUE(FindVersion(digest, SYNC_DOMAIN_FRIEND_LIST, version));
    EXPECT_EQ(version, 3u);
    ASSERT_TRUE(FindVersion(digest, SYNC_DOMAIN_TEAM, version));
    EXPECT_EQ(version, 17u);
    ASSERT_TRUE(FindVersion(digest, SYNC_DOMAIN_GUILD_ACTIVITY, version));
    EXPECT_EQ(version, 0u);
    EXPECT_FALSE(FindVersion(digest, SYNC_DOMAIN_GUILD, version)) << "缺席的域不应出现在 entries 里";
}

TEST(ParseSyncDigestFields, AcceptsMaxUint64Values)
{
    const Fields fields{
        {"g", "18446744073709551615"},
        {"2", "18446744073709551615"},
    };
    SyncDigestS2C digest;
    ASSERT_TRUE(PlayerLandingSystem::ParseSyncDigestFields(fields, &digest));
    EXPECT_EQ(digest.generation(), UINT64_MAX);
    uint64_t version = 0;
    ASSERT_TRUE(FindVersion(digest, SYNC_DOMAIN_FRIEND_REQUEST, version));
    EXPECT_EQ(version, UINT64_MAX);
}

TEST(ParseSyncDigestFields, GenerationOnlyHashIsValid)
{
    // 只有 g、没有任何域计数(例如 bump 调用没传域号):不违反"非空 hash 必有 g",照常接受。
    const Fields fields{{"g", "42"}};
    SyncDigestS2C digest;
    ASSERT_TRUE(PlayerLandingSystem::ParseSyncDigestFields(fields, &digest));
    EXPECT_EQ(digest.status(), SYNC_DIGEST_STATUS_OK);
    EXPECT_EQ(digest.generation(), 42u);
    EXPECT_EQ(digest.entries_size(), 0);
}

TEST(ParseSyncDigestFields, ClearsPreviousContentOfOutput)
{
    SyncDigestS2C digest;
    digest.set_status(SYNC_DIGEST_STATUS_UNAVAILABLE);
    digest.set_generation(7);
    auto *stale = digest.add_entries();
    stale->set_domain(SYNC_DOMAIN_GUILD);
    stale->set_version(99);

    const Fields fields{{"g", "100"}, {"5", "1"}};
    ASSERT_TRUE(PlayerLandingSystem::ParseSyncDigestFields(fields, &digest));
    EXPECT_EQ(digest.status(), SYNC_DIGEST_STATUS_OK);
    EXPECT_EQ(digest.generation(), 100u);
    ASSERT_EQ(digest.entries_size(), 1);
    EXPECT_EQ(digest.entries(0).domain(), SYNC_DOMAIN_TEAM_INVITE);
    EXPECT_EQ(digest.entries(0).version(), 1u);
}

// ---- 空应答(键不存在:从未 bump 或已过期)----

TEST(ParseSyncDigestFields, EmptyReplyMeansNoDigest)
{
    SyncDigestS2C digest;
    ASSERT_TRUE(PlayerLandingSystem::ParseSyncDigestFields(Fields{}, &digest));
    EXPECT_EQ(digest.status(), SYNC_DIGEST_STATUS_OK);
    EXPECT_EQ(digest.generation(), 0u) << "generation 0 = 摘要不存在,客户端据此全量拉";
    EXPECT_EQ(digest.entries_size(), 0);
}

TEST(ParseSyncDigestFields, NullOutputIsRejected)
{
    EXPECT_FALSE(PlayerLandingSystem::ParseSyncDigestFields(Fields{{"g", "1"}}, nullptr));
}

// ---- 缺 g ----

TEST(ParseSyncDigestFields, MissingGenerationWithDomainsIsMalformed)
{
    // BumpScript 在同一段原子脚本里先起种 g 再写域计数,非空 hash 必有 g。缺 g 时「重建必换 g」(L-3)不成立。
    const Fields fields{{"4", "3"}, {"6", "1"}};
    SyncDigestS2C digest;
    EXPECT_FALSE(PlayerLandingSystem::ParseSyncDigestFields(fields, &digest));
    EXPECT_EQ(digest.entries_size(), 0) << "返回 false 时输出必须被清空";
    EXPECT_EQ(digest.generation(), 0u);
}

TEST(ParseSyncDigestFields, MissingGenerationWithOnlyUnknownFieldsIsMalformed)
{
    const Fields fields{{"9999", "1"}};
    SyncDigestS2C digest;
    EXPECT_FALSE(PlayerLandingSystem::ParseSyncDigestFields(fields, &digest));
}

TEST(ParseSyncDigestFields, ZeroGenerationOnNonEmptyHashIsMalformed)
{
    // g 是 Redis 服务器毫秒,BumpScript 不会写出 0;0 与"摘要不存在"同义,不能和域计数并存。
    const Fields fields{{"g", "0"}, {"4", "3"}};
    SyncDigestS2C digest;
    EXPECT_FALSE(PlayerLandingSystem::ParseSyncDigestFields(fields, &digest));
}

// ---- 未知字段 ----

TEST(ParseSyncDigestFields, SkipsUnknownFields)
{
    const Fields fields{
        {"g", "500"},
        {"4", "2"},
        {"9999", "7"},       // 未来的域号:本版枚举里没有
        {"0", "1"},          // SYNC_DOMAIN_UNSPECIFIED 不是域
        {"04", "5"},         // 非规范十进制:同一个域不允许两种写法,按未知字段处理
        {"friend", "x"},     // 不是数字的字段名;未知字段的值不校验
        {"-1", "1"},
        {"99999999999999999999", "1"}, // 超过 uint64
    };
    SyncDigestS2C digest;
    ASSERT_TRUE(PlayerLandingSystem::ParseSyncDigestFields(fields, &digest));
    EXPECT_EQ(digest.generation(), 500u);
    ASSERT_EQ(digest.entries_size(), 1);
    EXPECT_EQ(digest.entries(0).domain(), SYNC_DOMAIN_TEAM);
    EXPECT_EQ(digest.entries(0).version(), 2u);
}

// ---- 非法值 ----

TEST(ParseSyncDigestFields, RejectsNonDecimalDomainValue)
{
    const std::vector<std::string> badValues{
        "",                       // 空串
        "abc",                    // 非数字
        "-1",                     // 负号
        "+1",                     // 正号
        " 1",                     // 前导空白
        "1 ",                     // 尾随空白
        "1.5",                    // 小数
        "0x10",                   // 十六进制
        "18446744073709551616",   // uint64 溢出
    };
    for (const auto &bad : badValues)
    {
        const Fields fields{{"g", "1000"}, {"1", bad}};
        SyncDigestS2C digest;
        EXPECT_FALSE(PlayerLandingSystem::ParseSyncDigestFields(fields, &digest)) << "value=\"" << bad << "\"";
        EXPECT_EQ(digest.entries_size(), 0) << "value=\"" << bad << "\"";
    }
}

TEST(ParseSyncDigestFields, RejectsNonDecimalGeneration)
{
    const std::vector<std::string> badValues{"", "abc", "-5", "12x", "18446744073709551616"};
    for (const auto &bad : badValues)
    {
        const Fields fields{{"g", bad}, {"4", "1"}};
        SyncDigestS2C digest;
        EXPECT_FALSE(PlayerLandingSystem::ParseSyncDigestFields(fields, &digest)) << "g=\"" << bad << "\"";
    }
}

TEST(ParseSyncDigestFields, RejectsDuplicateFields)
{
    // HGETALL 不会给出重复字段;出现即输入契约被破坏,不能挑一个当真。
    SyncDigestS2C digest;
    EXPECT_FALSE(PlayerLandingSystem::ParseSyncDigestFields(Fields{{"g", "1"}, {"g", "2"}}, &digest));
    EXPECT_FALSE(PlayerLandingSystem::ParseSyncDigestFields(Fields{{"g", "1"}, {"6", "1"}, {"6", "2"}}, &digest));
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
