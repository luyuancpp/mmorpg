#include "mysql_client.h"

#include <chrono>

#include "muduo/base/Logging.h"

namespace common
{

class RealResultRow : public ResultRow {
public:
    RealResultRow(Row row,  RowLength length, MYSQL_RES* res)
        : ResultRow(std::move(row), std::move(length)), res_(res) {}

    ~RealResultRow() override { mysql_free_result(res_); }

private:
    MYSQL_RES* res_;
};

void MysqlClient::Connect(const ConnectionParameters& database_info)
{
    connection_.reset(mysql_init(nullptr));

    uint32_t op = 1;
    mysql_options(connection(), MYSQL_OPT_RECONNECT, &op);

    MYSQL* res = mysql_real_connect(connection(),
        database_info.host_name_.c_str(),
        database_info.user_name_.c_str(),
        database_info.pass_word_.c_str(),
        database_info.database_name_.c_str(),
        database_info.port_,
        nullptr,
        CLIENT_FOUND_ROWS | CLIENT_MULTI_RESULTS);
    connection_->reconnect = true;
    if (nullptr == res)
    {
        return;
    }
    mysql_set_character_set(connection(), "utf8");
    conection_info_ = database_info;
}

void MysqlClient::Execute(const std::string& query)
{
#ifndef LOG_MYSQL_QUERY
    auto query_res = RealQuery(query);
#else
    auto query_res = LoggedRealQuery(query);
#endif//LOG_MYSQL_QUERY
    if (!query_res) {
        auto ec = query_res.error();

        std::stringstream ss;
        ss << "Error executing MySQL query \"";
        ss << "\": " << ec.message() << " (" << ec.value() << ")";
        LOG_INFO << ss.str();
        return;
    }
}

MysqlClient::ResultRowPtr MysqlClient::query_one(const std::string& query)
{
#ifndef LOG_MYSQL_QUERY
    auto query_res = RealQuery(query);
#else
    auto query_res = LoggedRealQuery(query);
#endif//LOG_MYSQL_QUERY
    if (!query_res) {
        auto ec = query_res.error();

        std::stringstream ss;
        ss << "Error executing MySQL query \"" ;
        ss << "\": " << ec.message() << " (" << ec.value() << ")";
        LOG_INFO << ss.str();
        return {};
    }

    // no resultset
    if (!query_res.value()) return {};

    auto* res = query_res.value().get();

    // get column info and give it to field validator,
    // which should throw if it doesn't like the columns
    unsigned int nfields = mysql_num_fields(res);
    MYSQL_FIELD* fields = mysql_fetch_fields(res);

    if (nfields == 0) return {};
   
    if (MYSQL_ROW row = mysql_fetch_row(res)) {
        unsigned long* lengths = mysql_fetch_lengths(res);
        std::vector<const char*> outrow(nfields);
        RowLength outlength(nfields);
        for (unsigned int i = 0; i < nfields; i++) {
            outrow[i] = row[i];
            outlength[i] = lengths[i];
        }

        return std::make_unique<RealResultRow>(outrow, outlength, query_res.value().release());
    }

    return {};
}

void MysqlClient::Query(const std::string& query, const RowProcessor& processor)
{
#ifndef LOG_MYSQL_QUERY
    auto query_res = RealQuery(query);
#else
    auto query_res = LoggedRealQuery(query);
#endif//LOG_MYSQL_QUERY
    if (!query_res) {
        auto ec = query_res.error();

        std::stringstream ss;
        ss << "Error executing MySQL query \"";
        ss << "\": " << ec.message() << " (" << ec.value() << ")";
        LOG_INFO << ss.str();
        return;
    }

    // no resultset
    if (!query_res.value()) return;

    auto* res = query_res.value().get();

    // get column info and give it to field validator,
    // which should throw if it doesn't like the columns
    unsigned int nfields = mysql_num_fields(res);

    MYSQL_FIELD* fields = mysql_fetch_fields(res);
    unsigned long* lengths = mysql_fetch_lengths(res);
    Row outrow;
    RowLength outlength;
    outrow.resize(nfields);
    outlength.resize(nfields);
    while (MYSQL_ROW row = mysql_fetch_row(res)) {
        for (unsigned int i = 0; i < nfields; i++) {
            outrow[i] = row[i];
            outlength[i] = lengths[i];
        }
        if (!processor(outrow, outlength)) break;
    }
}

MysqlClient::MysqlResultExpected MysqlClient::LoggedRealQuery(const std::string& q)
{
    using clock_type = std::chrono::steady_clock;

    auto start = clock_type::now();
    auto query_res = RealQuery(q);
    auto dur = clock_type::now() - start;
    auto msg =
        Address() + " (" +
        std::to_string(
            std::chrono::duration_cast<std::chrono::microseconds>(dur).count()) +
        " us)> " ;
    if (query_res) {
        auto const* res = query_res.value().get();

        msg += " // OK";
        if (res) {
            msg += " " + std::to_string(res->row_count) + " row" +
                (res->row_count != 1 ? "s" : "");
        }
    }
    else {
        auto err = query_res.error();
        msg += " // ERROR: " + std::to_string(err.value()) + " " + err.message();
    }
    LOG_INFO << msg;

    return query_res;
}

MysqlClient::MysqlResultExpected MysqlClient::RealQuery(const std::string& q)
{
    auto query_res = mysql_real_query(connection(), q.data(), (uint32_t)q.size());
    if ( 0 != query_res)
    {
        return stdx::make_unexpected(make_mysql_error_code(connection()));
    }
    MysqlResult res{ mysql_store_result(connection()) };
    if (!res)
    {
        // no error, but also no resultset
        if (mysql_errno(connection()) == 0) return {};

        return stdx::make_unexpected(make_mysql_error_code(connection()));
    }
#if defined(__SUNPRO_CC)
    // ensure sun-cc doesn't try to the copy-constructor on a move-only type
    return std::move(res);
#else
    return res;
#endif
}

}//namespace common;


