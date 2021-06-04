#ifndef COMMON_SRC_MYSQL_CLIENT_MYSQL_CLIENT_H_
#define COMMON_SRC_MYSQL_CLIENT_MYSQL_CLIENT_H_

#include <functional>
#include <string>
#include <memory>
#include <vector>

#include <mysql.h>

#include "src/util/expected.h"
#include "src/mysql_client/mysql_result.h"

namespace common
{
    //https://github.com/mysql/mysql-server/blob/8.0/router/src/router/src/common/mysql_session.cc
struct ConnectionParameters
{
    std::string host_name_;
    std::string user_name_;
    std::string pass_word_;
    std::string database_name_;
    uint32_t port_{ 0 };
};


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

static MysqlError make_mysql_error_code(unsigned int e) {
    return { e, ER_CLIENT(e), "HY000" };
}

static MysqlError make_mysql_error_code(MYSQL* m) {
    return { mysql_errno(m), mysql_error(m), mysql_sqlstate(m) };
}

class MYSQL_RES_Deleter {
public:
    void operator()(MYSQL_RES* res) { mysql_free_result(res); }
};

class MYSQL_Deleter {
public:
    void operator()(MYSQL* res) { mysql_close(res); }
};



class MysqlClient
{
public:
    using MysqlResult = std::unique_ptr<MYSQL_RES, MYSQL_RES_Deleter>;
    using MysqConnection = std::unique_ptr<MYSQL, MYSQL_Deleter>;
    using MysqlResultExpected = stdx::expected<MysqlResult, MysqlError>;
    using ResultRowPtr = std::unique_ptr<ResultRow>;
    using RowProcessor = std::function<bool(const Row&, const RowLength&)>;

    void Connect(const ConnectionParameters& database_info);
    void Execute(
        const std::string& query);
    ResultRowPtr query_one(
        const std::string& query);  
    void Query(
      const std::string& q, 
      const RowProcessor& processor);
private:
    MysqlResultExpected LoggedRealQuery(
        const std::string& q);
    inline MYSQL* connection() { return connection_.get(); }
    MysqlResultExpected RealQuery(
        const std::string& q);

    const std::string& Address() noexcept { return conection_info_.host_name_; }

    MysqConnection connection_;
    ConnectionParameters conection_info_;
};
}//namespace common

#endif//COMMON_SRC_MYSQL_CLIENT_MYSQL_CLIENT_H_
