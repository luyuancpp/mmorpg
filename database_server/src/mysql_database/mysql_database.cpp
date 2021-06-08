#include "mysql_database.h"

#include "muduo/base/Logging.h"

#include "mysql_database_table.pb.h"

using namespace common;

namespace database
{
void MysqlDatabase::Init()
{
    pb2db_.set_mysql(connection());
    pb2db_.AddTable(account_database_one_test::default_instance());
    for (auto& it : pb2db_.tables())
    {
        Execute(pb2db_.GetCreateTableSql(it.second.default_instance()));
        auto cb = std::bind(&Pb2DbSql::OnSelectTableColumnReturn, &it.second,
            std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
        Query(it.second.GetSelectColumn(), std::move(cb)); 
        auto alter_sql = pb2db_.GetAlterTableAddFieldSql(it.second.default_instance());
        if (!alter_sql.empty())
        {
            Execute(alter_sql);
        }        
    }
}

void MysqlDatabase::LoadOne(::google::protobuf::Message& message)
{
    auto sql = pb2db_.GetSelectAllSql(message);
    auto result = QueryOne(sql);
    FillMessageField(message, *result);
}

void MysqlDatabase::LoadOne(::google::protobuf::Message& message, const std::string& where_clause)
{
    auto sql = pb2db_.GetSelectSql(message, where_clause);
    auto result = QueryOne(sql);
    FillMessageField(message, *result);
}

void MysqlDatabase::SaveOne(const ::google::protobuf::Message& message)
{
    auto sql = pb2db_.GetInsertOnDupUpdateSql(message);
    QueryOne(sql);
}

}//namespace database

