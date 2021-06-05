#include <gtest/gtest.h>

#include "src/mysql_client/mysql_client.h"
#include "src/mysql_database/mysql_database.h"

#include "mysql_database_table.pb.h"

using namespace common;
using namespace database;

using DatabasePtr = std::unique_ptr<MysqlDatabase>;
DatabasePtr query_database;
ConnectionParameters query_database_param{ "127.0.0.1", "root" , "luyuan616586", "game" , 3306 };

TEST(RedisTest, SyncMessageLoad)
{
    MysqlDatabase client;
    client.Connect(query_database_param);
    client.Init();
}

TEST(RedisTest, QueryPb)
{
    account_database save_message;
    save_message.set_account("lu hailong");
    save_message.set_password("luyuan ");
    query_database->Save(save_message);
    account_database load_message;
    query_database->Load(load_message);
    EXPECT_EQ(save_message.account(), load_message.account());
    EXPECT_EQ(save_message.password(), load_message.password());
}

int main(int argc, char** argv)
{
    query_database = std::make_unique<MysqlDatabase>();
    query_database->Connect(query_database_param);
    query_database->Init();

    testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}