#ifndef COMMON_SRC_MYSQL_CLIENT_H_
#define COMMON_SRC_MYSQL_CLIENT_H_

#include <string>
#include <memory>

#include <mysql.h>

namespace common
{

struct MysqlDataBaseInfo
{
    std::string host_name_;
    std::string user_name_;
    std::string pass_word_;
    std::string database_name_;
    uint32_t port_{ 0 };
};

class MysqlClient
{
public:
    using MysqlPtr = std::shared_ptr<MYSQL>;
    void Connect(const MysqlDataBaseInfo& database_info);

private:
    MysqlPtr mysql_;
};
}//namespace common

#endif//COMMON_SRC_MYSQL_CLIENT_H_
