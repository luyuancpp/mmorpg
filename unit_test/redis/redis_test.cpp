#include <iostream>

#include "c2gw.pb.h"

#include <gtest/gtest.h>

#include "src/redis_client/redis_client.h"

using namespace common;

TEST(RedisTest, SyncMessageLoad)
{
    LoginRequest request;
    request.set_account("luhailon g");
    request.set_password("12 3");
    RedisClient c;
    c.Connect("127.0.0.1", 6379, 1, 1);
    c.Save(request);
    LoginRequest request_load;
    c.Load(request_load);
    std::cout << request_load.DebugString() << std::endl;
}

int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}