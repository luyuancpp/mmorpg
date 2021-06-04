#include <gtest/gtest.h>

#include "src/mysql_client/mysql_client.h"

using namespace common;

TEST(RedisTest, SyncMessageLoad)
{
    ConnectionParameters database_info;
    database_info.host_name_ = "127.0.0.1";
    database_info.user_name_ = "root";
    database_info.port_ = 3306;
    database_info.pass_word_ = "luyuan616586";
    database_info.database_name_ = "game";
    MysqlClient request;
    request.Connect(database_info);
}

int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}