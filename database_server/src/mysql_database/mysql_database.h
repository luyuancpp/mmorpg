#ifndef DATA_BASE_SRC_MYSQL_DATA_BASE_MYSQL_DATABASE_H_
#define DATA_BASE_SRC_MYSQL_DATA_BASE_MYSQL_DATABASE_H_

#include "src/mysql_client/mysql_client.h"
#include "src/pb2db/pb2dbsql.h"

namespace database
{
class MysqlDatabase : private common::Pb2DbTables, public common::MysqlClient
{
public:
    void Init();

    void Load(::google::protobuf::Message& message);
    void Save(const ::google::protobuf::Message& message);
};

}//namespace database

#endif // !DATA_BASE_SRC_MYSQL_DATA_BASE_MYSQL_DATABASE_H_
