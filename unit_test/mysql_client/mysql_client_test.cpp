#include <gtest/gtest.h>

#include "src/mysql_client/mysql_client.h"
#include "src/mysql_database/mysql_database.h"

using namespace common;
using namespace database;

TEST(RedisTest, SyncMessageLoad)
{
    ConnectionParameters database_info;
    database_info.host_name_ = "127.0.0.1";
    database_info.user_name_ = "root";
    database_info.port_ = 3306;
    database_info.pass_word_ = "luyuan616586";
    database_info.database_name_ = "game";
    MysqlDatabase client;
    client.Connect(database_info);
    client.Init();
}

int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}