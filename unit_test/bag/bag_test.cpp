#include <gtest/gtest.h>

#include "src/game_logic/bag/bag.h"

#include "src/return_code/error_code.h"

using namespace common;

TEST(BagTest, NullItem)
{
    Bag bag;
    EXPECT_EQ(nullptr, bag.GetItem(0));
}

TEST(BagTest, Add)
{
    Bag bag;
    auto item = CreateItem();
    EXPECT_EQ(RET_OK, bag.AddItem(item));
}

TEST(BagTest, Del)
{
    Bag bag;
    Guid guid{kInvalidGuid};
    EXPECT_EQ(RET_OK, bag.DelItem(guid));
}

TEST(BagTest, Update)
{
}

TEST(BagTest, Query)
{
    Bag bag;
    Guid guid{ kInvalidGuid };
    EXPECT_EQ(RET_OK, bag.DelItem(guid));
}

int main(int argc, char** argv)
{
    return RUN_ALL_TESTS();
}

