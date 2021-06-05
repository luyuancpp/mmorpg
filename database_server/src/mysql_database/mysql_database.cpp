#include "mysql_database.h"

#include "mysql_database_table.pb.h"

#include "src/pb2db/pb2dbsql.h"

namespace database
{
void MysqlDatabase::Init()
{
     common::Pb2DbTables::GetSingleton().AddTable(account_database::GetDescriptor()->full_name());
}

}//namespace database

