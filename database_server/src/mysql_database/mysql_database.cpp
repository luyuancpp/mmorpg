#include "mysql_database.h"

#include "mysql_database_table.pb.h"

using namespace common;

namespace database
{
void MysqlDatabase::Init()
{
    set_mysql(connection());
    AddTable(account_database::default_instance());
    for (auto& it : tables_)
    {
        Execute(GetCreateTableSql(it.second.default_instance()));
        auto cb = std::bind(&Pb2DbSql::OnSelectTableColumnReturn, &it.second,
            std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
        Query(it.second.GetSelectColumn(), std::move(cb)); 
        auto alter_sql = GetAlterTableAddFieldSql(it.second.default_instance());
        if (!alter_sql.empty())
        {
            Execute(alter_sql);
        }        
    }
}

void MysqlDatabase::Load(::google::protobuf::Message& message)
{
    auto sql = GetSelectAllSql(message);
    auto result = QueryOne(sql);
    fillMessageField(message, *result);
}

void MysqlDatabase::Save(const ::google::protobuf::Message& message)
{
    auto sql = GetReplaceSql(message);
    QueryOne(sql);
}

}//namespace database

