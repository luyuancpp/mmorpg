#include <string>
#include <cinttypes>

#include <gtest/gtest.h>
#include <muduo/base/FileUtil.h>

using namespace muduo;

TEST(FileUtilTest, ReadFile) 
{
	std::string result;
	int64_t size = 0;
#ifdef __linux__
	int err = FileUtil::readFile("/proc/self", 1024, &result, &size);
	printf("%d %zd %" PRIu64 "\n", err, result.size(), size);
	err = FileUtil::readFile("/proc/self", 1024, &result, NULL);
	printf("%d %zd %" PRIu64 "\n", err, result.size(), size);
	err = FileUtil::readFile("/proc/self/cmdline", 1024, &result, &size);
	printf("%d %zd %" PRIu64 "\n", err, result.size(), size);
	err = FileUtil::readFile("/dev/null", 1024, &result, &size);
	printf("%d %zd %" PRIu64 "\n", err, result.size(), size);
	err = FileUtil::readFile("/dev/zero", 1024, &result, &size);
	printf("%d %zd %" PRIu64 "\n", err, result.size(), size);
	err = FileUtil::readFile("/notexist", 1024, &result, &size);
	printf("%d %zd %" PRIu64 "\n", err, result.size(), size);
	err = FileUtil::readFile("/dev/zero", 102400, &result, &size);
	printf("%d %zd %" PRIu64 "\n", err, result.size(), size);
	err = FileUtil::readFile("/dev/zero", 102400, &result, NULL);
	printf("%d %zd %" PRIu64 "\n", err, result.size(), size);
#endif // __linux__

#ifdef WIN32
	int err = FileUtil::readFile("ReadMe.txt", 1024, &result, &size);
	printf("%d %zd %" PRIu64 "\n", err, result.size(), size);
	err = FileUtil::readFile("/dev/zero", 102400, &result, NULL);
	printf("%d %zd %" PRIu64 "\n", err, result.size(), size);
#endif // WIN32

}

int main(int argc, char **argv)
{
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}

