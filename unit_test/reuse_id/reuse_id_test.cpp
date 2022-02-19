#include <gtest/gtest.h>

#include "src/game_logic/reuse_id/reuse_id.h"
#include "src/reuse_game_id/reuse_game_id.h"

using namespace common;

TEST(Reuse, SetType)
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

TEST(Reuse, UnorderSetType)
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

//重启完毕以后没gameserver连接上来
TEST(Reuse, ReuseDepolyStartNoGameserver)
{
    deploy::ReuseGameServerId rgs;
    uint32_t max_id = 100;
    rgs.set_size(max_id);
    rgs.OnDbLoadComplete();
    rgs.ScanOver();
    for (uint32_t i = 0; i < max_id; ++i)
    {
        EXPECT_TRUE(rgs.Create() < max_id);
    }    
    EXPECT_EQ(rgs.Create(), max_id);
}

//重启完毕以后没ameserver全部连接上来
TEST(Reuse, DepolyStartGameserverReconnectScanOver)
{
    deploy::ReuseGameServerId rgs;
    uint32_t max_id = 100;
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
        EXPECT_TRUE(rgs.Create() < max_id);
    }
    EXPECT_EQ(rgs.Create(), max_id);
}

//重启扫描完毕以后Gameserver部分连接上来
TEST(Reuse, DepolyStartGameserverReconnectNoScanOver)
{
    deploy::ReuseGameServerId rgs;
    uint32_t max_id = 100;
    rgs.set_size(max_id);
    rgs.OnDbLoadComplete();
    uint32_t half = max_id / 2;
    //一般连接上来了
    for (uint32_t i = 0; i < half; ++i)
    {
        rgs.Emplace(std::to_string(i), i);
    }

    //新连接应该用新id因为没有扫描完毕
    for (uint32_t i = 0; i < half; ++i)
    {
        EXPECT_TRUE(rgs.Create() >= max_id);
    }

    //扫描完毕以后，有一半是连接不上的
    rgs.ScanOver();
    for (uint32_t i = 0; i < half; ++i)
    {
        EXPECT_TRUE(rgs.Create() < max_id);
    }
    EXPECT_EQ(rgs.Create(), max_id + half);
}


int32_t main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

