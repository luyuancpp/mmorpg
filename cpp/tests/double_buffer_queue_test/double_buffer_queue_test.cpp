#include <gtest/gtest.h>

#include <algorithm>
#include <set>
#include <thread>
#include <vector>

#include "core/utils/data_structures/double_buffer_queue.h"

TEST(DoubleBufferQueueTest, TakeEmptyReturnsFalse)
{
    DoubleBufferQueue<int> queue;
    int value = 0;
    EXPECT_FALSE(queue.take(value));
    EXPECT_TRUE(queue.empty());
    EXPECT_EQ(queue.size(), 0);
}

TEST(DoubleBufferQueueTest, PreservesOrderSingleProducer)
{
    DoubleBufferQueue<int> queue;
    queue.put(1);
    queue.put(2);
    queue.put(3);

    int value = 0;
    ASSERT_TRUE(queue.take(value));
    EXPECT_EQ(value, 1);
    ASSERT_TRUE(queue.take(value));
    EXPECT_EQ(value, 2);
    ASSERT_TRUE(queue.take(value));
    EXPECT_EQ(value, 3);
    EXPECT_FALSE(queue.take(value));
}

TEST(DoubleBufferQueueTest, CanProcessFixedBudgetPerFrame)
{
    DoubleBufferQueue<int> queue;
    for (int i = 0; i < 10; ++i)
    {
        queue.put(i);
    }

    std::vector<int> processed;
    constexpr int kPerFrame = 3;

    for (int frame = 0; frame < 4; ++frame)
    {
        for (int i = 0; i < kPerFrame; ++i)
        {
            int value = -1;
            if (!queue.take(value))
            {
                break;
            }
            processed.push_back(value);
        }
    }

    EXPECT_EQ(processed.size(), 10);
    for (int i = 0; i < 10; ++i)
    {
        EXPECT_EQ(processed[i], i);
    }
    EXPECT_TRUE(queue.empty());
}

TEST(DoubleBufferQueueTest, SupportsMultiProducerSingleConsumer)
{
    DoubleBufferQueue<int> queue;
    constexpr int kPerProducer = 200;

    std::thread t1([&queue]() {
        for (int i = 0; i < kPerProducer; ++i)
        {
            queue.put(i);
        }
    });

    std::thread t2([&queue]() {
        for (int i = 0; i < kPerProducer; ++i)
        {
            queue.put(10000 + i);
        }
    });

    t1.join();
    t2.join();

    std::multiset<int> values;
    int value = 0;
    while (queue.take(value))
    {
        values.insert(value);
    }

    EXPECT_EQ(values.size(), static_cast<size_t>(kPerProducer * 2));
    for (int i = 0; i < kPerProducer; ++i)
    {
        EXPECT_EQ(values.count(i), 1u);
        EXPECT_EQ(values.count(10000 + i), 1u);
    }
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
