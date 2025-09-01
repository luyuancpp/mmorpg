#include <gtest/gtest.h>
#include <muduo/net/ZlibStream.h>

#include <muduo/base/Logging.h>


#include <stdio.h>

TEST(ZlibStreamUnitTest, testZlibOutputStream)
{
	muduo::net::Buffer output;
	{
		muduo::net::ZlibOutputStream stream(&output);
		EXPECT_EQ(output.readableBytes(), (size_t)0);
	}
	EXPECT_EQ(output.readableBytes(), (size_t)8);
}

TEST(ZlibStreamUnitTest, testZlibOutputStream1)
{
	muduo::net::Buffer output;
	muduo::net::ZlibOutputStream stream(&output);
	EXPECT_EQ(stream.zlibErrorCode(), Z_OK);
	stream.finish();
	EXPECT_EQ(stream.zlibErrorCode(), Z_STREAM_END);
}

TEST(ZlibStreamUnitTest, testZlibOutputStream2)
{
	muduo::net::Buffer output;
	muduo::net::ZlibOutputStream stream(&output);
	EXPECT_EQ(stream.zlibErrorCode(), Z_OK);
	EXPECT_TRUE(stream.write("01234567890123456789012345678901234567890123456789"));
	stream.finish();
	// printf("%zd\n", output.readableBytes());
	EXPECT_EQ(stream.zlibErrorCode(), Z_STREAM_END);
}

TEST(ZlibStreamUnitTest, testZlibOutputStream3)
{
	muduo::net::Buffer output;
	muduo::net::ZlibOutputStream stream(&output);
	EXPECT_EQ(stream.zlibErrorCode(), Z_OK);
	for (int i = 0; i < 1024 * 1024; ++i)
	{
		EXPECT_TRUE(stream.write("01234567890123456789012345678901234567890123456789"));
	}
	stream.finish();
	// printf("total %zd\n", output.readableBytes());
	EXPECT_EQ(stream.zlibErrorCode(), Z_STREAM_END);
}

TEST(ZlibStreamUnitTest, testZlibOutputStream4)
{
	muduo::net::Buffer output;
	muduo::net::ZlibOutputStream stream(&output);
	EXPECT_EQ(stream.zlibErrorCode(), Z_OK);
	muduo::string input;
	for (int i = 0; i < 32768; ++i)
	{
		input += "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_-"[rand() % 64];
	}

	for (int i = 0; i < 10; ++i)
	{
		EXPECT_TRUE(stream.write(input));
	}
	stream.finish();
	// printf("total %zd\n", output.readableBytes());
	EXPECT_EQ(stream.zlibErrorCode(), Z_STREAM_END);
}

TEST(ZlibStreamUnitTest, testZlibOutputStream5)
{
	muduo::net::Buffer output;
	muduo::net::ZlibOutputStream stream(&output);
	EXPECT_EQ(stream.zlibErrorCode(), Z_OK);
	muduo::string input(1024 * 1024, '_');
	for (int i = 0; i < 64; ++i)
	{
		EXPECT_TRUE(stream.write(input));
	}
	printf("bufsiz %d\n", stream.internalOutputBufferSize());
	LOG_INFO << "total_in " << stream.inputBytes();
	LOG_INFO << "total_out " << stream.outputBytes();
	stream.finish();
	printf("total %zd\n", output.readableBytes());
	EXPECT_EQ(stream.zlibErrorCode(), Z_STREAM_END);
}
int main(int argc, char **argv)
{

	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}