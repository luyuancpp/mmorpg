#include <gtest/gtest.h>

#include "src/game_logic/reuse_id/reuse_id.h"
#include "src/reuse_game_id/reuse_game_id.h"

using namespace common;

TEST(RedisTest, ReuseSetType)
{
    ReuseId<uint32_t, std::set<uint32_t>, UINT32_MAX> reuseid;
    uint32_t test_size = 100000;
    for (uint32_t i = 0; i < test_size; ++i)
    {
        EXPECT_EQ(i, reuseid.Create());
    }

    for (uint32_t i = 0; i < test_size; ++i)
    {
       reuseid.Destroy(i);
    }
    for (uint32_t i = 0; i < test_size; ++i)
    {
        EXPECT_EQ(i, reuseid.Create());
    }
}

TEST(RedisTest, ReuseUnorderSetType)
{
    ReuseId<uint32_t, std::unordered_set<uint32_t>, UINT32_MAX> reuseid;
    uint32_t test_size = 100000;
    for (uint32_t i = 0; i < test_size; ++i)
    {
        EXPECT_EQ(i, reuseid.Create());
    }

    for (uint32_t i = 0; i < test_size; ++i)
    {
        reuseid.Destroy(i);
    }
    for (uint32_t i = 0; i < test_size; ++i)
    {
        reuseid.Create();
    }
    EXPECT_EQ(test_size, reuseid.Create());
}

TEST(RedisTest, ReuseDepolyStartNoGameserver)
{
    deploy::ReuseGameServerId rgs;
    deploy::ReuseGameServerId::FreeList fl;
    uint32_t max_id = 100;
    for (uint32_t i = 0; i < max_id; ++i)
    {
        fl.insert({ i,true });
    }
    rgs.set_free_list(fl);
    rgs.set_size(max_id);
    rgs.OnDbLoadComplete();
    rgs.ScanOver();
    for (uint32_t i = 0; i < max_id; ++i)
    {
        EXPECT_TRUE(rgs.CreateGameId() < max_id);
    }    
    EXPECT_EQ(rgs.CreateGameId(), max_id);
}

TEST(RedisTest, ReuseDepolyStartGameserverReconnectScanOver)
{
    deploy::ReuseGameServerId rgs;
    deploy::ReuseGameServerId::FreeList fl;
    uint32_t max_id = 100;
    for (uint32_t i = 0; i < max_id; ++i)
    {
        fl.insert({ i,true });
    }
    rgs.set_free_list(fl);
    rgs.set_size(max_id);
    rgs.OnDbLoadComplete();
    uint32_t half = max_id / 2;
    for (uint32_t i = 0; i < half; ++i)
    {
        rgs.Emplace(std::to_string(i), i);
    }
 
    rgs.ScanOver();
    for (uint32_t i = 0; i < half; ++i)
    {
        EXPECT_TRUE(rgs.CreateGameId() < max_id);
    }
    EXPECT_EQ(rgs.CreateGameId(), max_id);
}

TEST(RedisTest, ReuseDepolyStartGameserverReconnectNoScanOver)
{
    deploy::ReuseGameServerId rgs;
    deploy::ReuseGameServerId::FreeList fl;
    uint32_t max_id = 100;
    for (uint32_t i = 0; i < max_id; ++i)
    {
        fl.insert({ i,true });
    }
    rgs.set_free_list(fl);
    rgs.set_size(max_id);
    rgs.OnDbLoadComplete();
    uint32_t half = max_id / 2;
    for (uint32_t i = 0; i < half; ++i)
    {
        rgs.Emplace(std::to_string(i), i);
    }
    
    for (uint32_t i = 0; i < half ; ++i)
    {
        EXPECT_TRUE(rgs.CreateGameId() >= max_id);
    }

    rgs.ScanOver();
    for (uint32_t i = 0; i < half; ++i)
    {
        EXPECT_TRUE(rgs.CreateGameId() < max_id);
    }
    EXPECT_EQ(rgs.CreateGameId(), max_id + half);
}

int32_t main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

