#include "mysql_database.h"

#include "mysql_database_table.pb.h"

using namespace common;

namespace database
{
void MysqlDatabase::Init()
{
    AddTable(account_database::default_instance());
    for (auto& it : tables_)
    {
        Execute(GetCreateTableSql(it.second.default_instance()));
        auto cb = std::bind(&Pb2DbSql::OnSelectColumnReturn, &it.second,
            std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
        Query(it.second.GetSelectColumn(), std::move(cb)); 
        auto alter_sql = GetAlterTableAddFieldSql(it.second.default_instance());
        if (!alter_sql.empty())
        {
            Execute(alter_sql);
        }        
    }
}

}//namespace database

