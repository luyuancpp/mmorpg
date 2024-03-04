#include "mysql_database.h"

#include "muduo/base/Logging.h"

#include "common_proto/mysql_database_test.pb.h"

void MysqlDatabase::set_auto_increment(const ::google::protobuf::Message& message_default_instance, uint64_t auto_increment)
{
	pb2db_.set_auto_increment(message_default_instance, auto_increment);
}

void MysqlDatabase::CreateMysqlTable(const ::google::protobuf::Message& message_default_instance)
{
	pb2db_.CreateMysqlTable(message_default_instance);
}

void MysqlDatabase::Init()
{
    pb2db_.set_mysql(connection());
    
    for (auto& it : pb2db_.tables())
    {
        Execute(pb2db_.GetCreateTableSql(it.second.default_instance()));
        auto cb = std::bind(&Message2MysqlSqlStmt::OnSelectTableColumnReturnSqlStmt, &it.second,
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
    if (nullptr == result)
    {
        return;
    }
    ParseFromString(message, *result);
}

void MysqlDatabase::LoadOne(::google::protobuf::Message& message, const std::string& where_clause)
{
    auto sql = pb2db_.GetSelectSql(message, where_clause);
    auto result = QueryOne(sql);
    if (nullptr == result)
    {
        return;
    }
    ParseFromString(message, *result);
}

void MysqlDatabase::SaveOne(const ::google::protobuf::Message& message)
{
    auto sql = pb2db_.GetInsertOnDupUpdateSql(message);
    QueryOne(sql);
}

void MysqlDatabase::Delete(const ::google::protobuf::Message& message)
{
    auto sql = pb2db_.GetDeleteSql(message);
    QueryOne(sql);
}

void MysqlDatabase::Delete(const ::google::protobuf::Message& message, const std::string& where_clause)
{
    auto sql = pb2db_.GetDeleteSql(message, where_clause);
    QueryOne(sql);
}


