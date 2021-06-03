#include "mysql_client.h"

namespace common
{

void MysqlClient::Connect(const MysqlDataBaseInfo& database_info)
{
    mysql_ = std::shared_ptr<MYSQL>(mysql_init(nullptr), mysql_close);

    uint32_t op = 1;
    mysql_options(mysql_.get(), MYSQL_OPT_RECONNECT, &op);

    MYSQL* res = mysql_real_connect(mysql_.get(),
        database_info.host_name_.c_str(),
        database_info.user_name_.c_str(),
        database_info.pass_word_.c_str(),
        database_info.database_name_.c_str(),
        database_info.port_,
        nullptr,
        CLIENT_FOUND_ROWS | CLIENT_MULTI_RESULTS);

    if (nullptr == res)
    {
        return;
    }
    mysql_set_character_set(mysql_.get(), "utf8");
}

}//namespace common;


