#ifndef COMMON_SRC_MYSQL_CLIENT_MYSQL_CLIENT_H_
#define COMMON_SRC_MYSQL_CLIENT_MYSQL_CLIENT_H_

#include <functional>
#include <string>
#include <memory>
#include <vector>

#include <mysql.h>

#include "src/util/expected.h"
#include "src/mysql_client/mysql_result.h"

#include "muduo/base/Logging.h"

#include "common.pb.h"
#include "deploy_database_table.pb.h"

namespace common
{
//https://github.com/mysql/mysql-server/blob/8.0/router/src/router/src/common/mysql_session.cc

class MysqlError {
public:
    MysqlError(unsigned int code, std::string message, std::string sql_state)
        : code_{ code },
        message_{ std::move(message) },
        sql_state_{ std::move(sql_state) } {}

    operator bool() { return code_ != 0; }

    std::string message() const { return message_; }
    std::string sql_state() const { return sql_state_; }
    unsigned int value() const { return code_; }

private:
    unsigned int code_;
    std::string message_;
    std::string sql_state_;
};

class MYSQL_RES_Deleter {
public:
    void operator()(MYSQL_RES* res) { mysql_free_result(res); }
};

class MysqlClient
{
public:
    using MysqlResult = std::unique_ptr<MYSQL_RES, MYSQL_RES_Deleter>;
    using MysqlResultExpected = stdx::expected<MysqlResult, MysqlError>;
    using ResultRowPtr = std::unique_ptr<ResultRow>;
    using RowProcessor = std::function<bool(const MYSQL_ROW&, const unsigned long*, uint32_t)>;
    using ResultRowProcessor = std::function<bool(const ResultRowPtr&)>;

    ~MysqlClient() { mysql_close(connection_); };

    template <typename ConnectionParamClass>
    void Connect(const ConnectionParamClass& database_info)
    {
        connection_ = mysql_init(nullptr);
        uint32_t op = 1;
        mysql_options(connection(), MYSQL_OPT_RECONNECT, &op);
        auto p_mysql = mysql_real_connect(connection(),
            database_info.db_host().c_str(),
            database_info.db_user().c_str(),
            database_info.db_password().c_str(),
            database_info.db_dbname().c_str(),
            database_info.db_port(),
            nullptr,
            CLIENT_FOUND_ROWS | CLIENT_MULTI_RESULTS);
        if (nullptr == p_mysql)
        {
            LOG_FATAL << "mysql_real_connect\"" << database_info.DebugString().c_str() << ")";
            return;
        }

        connection_->reconnect = true;
        mysql_set_character_set(connection(), "utf8");
    }

    void Execute( const std::string& query);
    ResultRowPtr QueryOne(const std::string& query);  
    void Query( const std::string& query, const RowProcessor& processor);

    void QueryResultRowProcessor( const std::string& query, const ResultRowProcessor& processor);

    uint64_t LastInsertId();
protected:
    inline MYSQL* connection() { return connection_; }
private:
    MysqlResultExpected LoggedRealQuery(
        const std::string& q);
    MysqlResultExpected RealQuery(
        const std::string& q);

    MYSQL* connection_;
};
}//namespace common

#endif//COMMON_SRC_MYSQL_CLIENT_MYSQL_CLIENT_H_
