#include <functional>
#include <thread>

#include <muduo/base/CountDownLatch.h>
#include <gtest/gtest.h>

using namespace std;

class CountDownLatchTest : public testing::Test {
protected:

	CountDownLatchTest()
		: latch_(5),
		  count_(5)
	{
	}

	virtual void SetUp()
	{
		for (int i = 0; i < count_; ++i)
		{
			m_vThreads.push_back(std::move(thread(bind(&CountDownLatchTest::ThreadFunc, this))));
		}

	}



	virtual void TearDown()
	{
		m_vThreads.clear();
	}

	virtual ~CountDownLatchTest() { }


	void Run()
	{
		latch_.wait();

	}

	void JoinAll()
	{
		for_each(m_vThreads.begin(), m_vThreads.end(), bind(&thread::join, placeholders::_1));
	}

	void ThreadFunc()
	{
		latch_.countDown();
	}


	muduo::CountDownLatch latch_;
	vector<std::thread> m_vThreads;
	int count_;

};

TEST_F(CountDownLatchTest, TestAfterRunCountEqZero) {
	Run();
	JoinAll();
	EXPECT_EQ(0, latch_.getCount());
}

int main(int argc, char **argv)
{
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}


