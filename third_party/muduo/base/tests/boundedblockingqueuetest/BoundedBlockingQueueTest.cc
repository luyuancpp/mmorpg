#include <algorithm>
#include <cstdio>
#include <functional>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>
#include <muduo/base/BoundedBlockingQueue.h>


using namespace std;

class BBQTest : public testing::Test {
protected:

	BBQTest()
		: m_vQueue(20),
		m_vThreads()
	{
	}

	virtual void SetUp()
	{
		for (size_t i = 0; i < 5; ++i)
		{
			m_vThreads.push_back(std::move(thread(bind(&BBQTest::Produce, this))));
		}

		for (size_t i = 0; i < 5; ++i)
		{
			m_vThreads.push_back(std::move(thread(bind(&BBQTest::Consume, this))));
		}
	}

	void JoinAll()
	{
		for_each(m_vThreads.begin(), m_vThreads.end(), bind(&thread::join, placeholders::_1));
	}

	virtual void TearDown()
	{
		m_vThreads.clear();
	}

	virtual ~BBQTest() { }


	void Produce()
	{


		for (size_t i = 0; i < 1000; i++)
		{
			char buf[32];
			snprintf(buf, sizeof buf, "hello %zd", i);
			m_vQueue.put(buf);
			//printf("tid=%d, put data = %s, size = %zd\n", std::this_thread::get_id(), buf, m_vQueue.size());
		}



	}

	void Consume()
	{
		for (size_t i = 0; i < 1000; i++)
		{
			std::string d(m_vQueue.take());
			//printf("tid=%d, get data = %s, size = %zd\n", std::this_thread::get_id(), d.c_str(), m_vQueue.size());
		}
	}


	muduo::BoundedBlockingQueue<string> m_vQueue;
    vector<std::thread> m_vThreads;
};

TEST_F(BBQTest, TestConsumeNumEqProduceNum) {
	JoinAll();
	EXPECT_EQ(0, (int)m_vQueue.size());
}

int main(int argc, char **argv)
{
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}


