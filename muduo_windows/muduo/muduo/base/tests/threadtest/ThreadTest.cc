#include <atomic>
#include <iostream>       // std::cout, std::endl
#include <thread>         // std::this_thread::sleep_for
#include <chrono>         // std::chrono::seconds

#include <gtest/gtest.h>

#include <muduo/base/CurrentThread.h>

std::atomic<int> numCreated;

void mysleep(int seconds)
{
	std::this_thread::sleep_for(std::chrono::seconds(1));
}

void threadFunc()
{
	printf("tid=%d\n", muduo::CurrentThread::tid());
	++numCreated;
}

void threadFunc2(int x)
{
	printf("tid=%d, x=%d\n", muduo::CurrentThread::tid(), x);
	++numCreated;
}

void threadFunc3()
{
	printf("tid=%d\n", muduo::CurrentThread::tid());
	++numCreated;
	mysleep(1);
}

class Foo
{
public:
	explicit Foo(double x)
		: x_(x)
	{
	}

	void memberFunc()
	{
		printf("tid=%d, Foo::x_=%f\n", muduo::CurrentThread::tid(), x_);
	}

	void memberFunc2(const std::string& text)
	{
		printf("tid=%d, Foo::x_=%f, text=%s\n", muduo::CurrentThread::tid(), x_, text.c_str());
	}

private:
	double x_;
};

TEST(ThreadTest, FooTest)
{
	printf("tid=%d\n", muduo::CurrentThread::tid());

	std::thread t1(threadFunc);
	t1.join();

	std::thread t2(std::bind(threadFunc2, 42),
		"thread for free function with argument");
	t2.join();

	Foo foo(87.53);
	std::thread t3(std::bind(&Foo::memberFunc, &foo),
		"thread for member function without argument");

	t3.join();

	std::thread t4(std::bind(&Foo::memberFunc2, std::ref(foo), std::string("Shuo Chen")));
	t4.join();

	{
		std::thread t5(threadFunc3);
		// t5 may destruct eariler than thread creation.
		t5.join();
	}
	mysleep(2);
	{
		std::thread t6(threadFunc3);
		mysleep(2);
		t6.join();
		// t6 destruct later than thread creation.
	}
	std::this_thread::sleep_for(std::chrono::seconds(2));
	printf("number of created threads %d\n", numCreated.load(std::memory_order_relaxed));
}




int main(int argc, char **argv)
{
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
