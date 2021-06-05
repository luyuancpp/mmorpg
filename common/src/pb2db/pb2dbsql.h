#ifndef COMMON_SRC_PB2DB_PB2DBSQL_H_
#define COMMON_SRC_PB2DB_PB2DBSQL_H_

#include <cctype>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "mysql.h"

#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

#include "src/common_type/common_type.h"
#include "src/mysql_client/mysql_result.h"

namespace common
{
    class Pb2DbSql
    {
    public:
        using FieldsType = std::unordered_set<std::string>;

        Pb2DbSql(const ::google::protobuf::Message& message_default_instance)
            : default_instance_(message_default_instance)
        {

        }

        std::string GetCreateTableSql();
        std::string GetAlterTableAddFieldSql(::google::protobuf::Message& message);
        std::string GetInsertSql(::google::protobuf::Message& message, MYSQL* mysql);
        std::string GetSelectSql(::google::protobuf::Message& message, const std::string& key, const std::string& val);
        std::string GetSelectSql(::google::protobuf::Message& message, const std::string& where_clause);
        std::string GetSelectAllSql(::google::protobuf::Message& message);
        std::string GetInsertOnDupUpdateSql(::google::protobuf::Message& message, MYSQL* mysql);
        std::string GetDeleteSql(::google::protobuf::Message& message);
        std::string GetDeleteSql(::google::protobuf::Message& message, const std::string& where_clause);
        std::string GetReplaceSql(::google::protobuf::Message& message, MYSQL* mysql);
        std::string GetUpdateSql(::google::protobuf::Message& message, MYSQL* mysql);
        std::string GetUpdateSql( ::google::protobuf::Message& message, MYSQL* mysql, std::string where_clause);
        std::string GetTruncateSql(::google::protobuf::Message& message);
        void EscapeString(std::string& str, MYSQL* mysql);
        void InsertFiled(const std::string& field)
        {
            fileds_.insert(field);
        }

        const ::google::protobuf::Message& default_instance() { return default_instance_; }
        inline const std::string& GetTypeName();

        static void fillMessageField(::google::protobuf::Message& message, const ResultRow& row);
    private:
        std::string getUpdateSet(MYSQL* mysql, ::google::protobuf::Message& message);

        FieldsType fileds_;
        StringV primarykeys_;
        StringV indexes_;
        StringV unique_keys_;
        std::string foreign_keys_;
        std::string foreign_references_;
        std::string auto_increase_key_;
        const ::google::protobuf::Message& default_instance_;
    };

    class Pb2DbTables
    {
    public:
        using PbSqlMap = std::unordered_map<std::string, Pb2DbSql>;

        std::string GetCreateTableSql(const ::google::protobuf::Message& message);
        std::string GetAlterTableAddFieldSql(::google::protobuf::Message& message);
        std::string GetInsertSql(::google::protobuf::Message& message, MYSQL* mysql);
        std::string GetReplaceSql(::google::protobuf::Message& message, MYSQL* mysql);
        std::string GetInsertOnDupUpdateSql(::google::protobuf::Message& message, MYSQL* mysql);
        std::string GetUpdateSql(::google::protobuf::Message& message, MYSQL* mysql);
        std::string GetUpdateSql(::google::protobuf::Message& message, MYSQL* mysql, std::string where_clause);
        std::string GetSelectSql(::google::protobuf::Message& message, const std::string& key, const std::string& val);
        std::string GetSelectSql(::google::protobuf::Message& message, const std::string& where_clause);
        std::string GetSelectAllSql(::google::protobuf::Message& message);
        std::string GetDeleteSql(::google::protobuf::Message& message);
        std::string GetDeleteSql(::google::protobuf::Message& message, std::string where_clause);
        std::string GetTruncateSql(::google::protobuf::Message& message);

        static Pb2DbTables& GetSingleton()
        {
            static Pb2DbTables singleton;
            return singleton;
        }

        void InsertFiled(const std::string& tbname, const std::string& field);

        void AddTable(const ::google::protobuf::Message& message_default_instance);
protected:
        PbSqlMap tables_;
    };

}//namespace common

#endif // !COMMON_SRC_PB2DB_PB2DBSQL_H_
