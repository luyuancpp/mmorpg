#include <muduo/base/Exception.h>
#include <gtest/gtest.h>

class Bar
{
public:
	void test()
	{
		throw muduo::Exception("oops");
	}
};

void foo()
{
	Bar b;
	b.test();
}

TEST(ExceptionTest, PrintStack) {
	try
	{
		foo();
	}
	catch (const muduo::Exception& ex)
	{
		printf("reason: %s\n", ex.what());
		printf("stack trace: %s\n", ex.stackTrace());
	}
}

int main(int argc, char **argv)
{
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}


