#include <gtest/gtest.h>

#include "src/mysql_client/mysql_client.h"
#include "src/mysql_database/mysql_database.h"

#include "mysql_database_table.pb.h"

using namespace common;
using namespace database;

using DatabasePtr = std::unique_ptr<MysqlDatabase>;
DatabasePtr query_database;
ConnectionParameters query_database_param{ "127.0.0.1", "root" , "luyuan616586", "game" , 3306 };

TEST(MysqlClientTest, ConectMysql)
{
    MysqlDatabase client;
    client.Connect(query_database_param);
    client.Init();
}

TEST(MysqlClientTest, QueryOptionMessage)
{
    account_database_one_test save_message;
    save_message.set_account("lu hailong");
    save_message.set_password("luyuan ");
    query_database->SaveOne(save_message);
    account_database_one_test load_message;
    query_database->LoadOne(load_message);
    EXPECT_EQ(save_message.account(), load_message.account());
    EXPECT_EQ(save_message.password(), load_message.password());
    account_database_one_test load_message_where;
    query_database->LoadOne(load_message_where, "account = 'lu hailong'");
    EXPECT_EQ(save_message.account(), load_message_where.account());
    EXPECT_EQ(save_message.password(), load_message_where.password());
}

TEST(MysqlClientTest, QueryRepeatedMessage)
{
    account_database_all_test save_message;
    auto first =  save_message.mutable_account_password()->Add();
    first->set_account("lu hailong1");
    first->set_password("luyuan ");
    auto second = save_message.mutable_account_password()->Add();
    second->set_account("luh ailong1");
    second->set_password("lu yuan ");
    query_database->SaveAll<::account_database_one_test>(save_message);
    account_database_all_test load_message;
    query_database->LoadAll<::account_database_one_test>(load_message);
}

int main(int argc, char** argv)
{
    query_database = std::make_unique<MysqlDatabase>();
    query_database->Connect(query_database_param);
    query_database->Init();

    testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}