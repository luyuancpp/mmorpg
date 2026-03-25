#include <gtest/gtest.h>

#include "core/utils/file/file2string.h"

TEST(File2StringTest, ReadFileContent)
{
	File2String("test.txt");
}

int32_t main(int argc, char** argv)
{
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}