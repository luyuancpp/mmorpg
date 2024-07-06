#include <gtest/gtest.h>

#include "src/common_type/common_type.h"
#include "src/util/file2string.h"

TEST(GS, ReadFile2String)
{
   common::File2String("test.txt");
}

int32_t main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}