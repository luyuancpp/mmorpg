#ifndef COMMON_SRC_PB2DB_DB_TABLE_H_
#define COMMON_SRC_PB2DB_DB_TABLE_H_

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

namespace common
{
    struct MysqlField
    {
        char* field_chars_{ nullptr };
        int32_t field_len_{ 0 };
    };

    class DBTable
    {
    public:
        using FieldsType = std::unordered_set<std::string>;

        std::string GetCreateTableSql(::google::protobuf::Message& pb);
        std::string GetAlterTableAddFieldSql(::google::protobuf::Message& pb);
        std::string GetInsertSql(MYSQL* conn, ::google::protobuf::Message& pb);
        std::string GetSelectSql(::google::protobuf::Message& pb, const std::string& key, const std::string& val);
        std::string GetSelectSql(::google::protobuf::Message& pb, const std::string& where_clause);
        std::string GetSelectAllSql(::google::protobuf::Message& pb);
        std::string GetInsertOnDupUpdateSql(MYSQL* conn, ::google::protobuf::Message& pb);
        std::string GetDeleteSql(::google::protobuf::Message& pb);
        std::string GetDeleteSql(::google::protobuf::Message& pb, const std::string& where_clause);
        std::string GetReplaceSql(MYSQL* conn, ::google::protobuf::Message& pb);
        std::string GetUpdateSql(MYSQL* conn, ::google::protobuf::Message& pb);
        std::string GetUpdateSql(MYSQL* _mysql, ::google::protobuf::Message& pb, std::string where_clause);
        std::string GetTruncateSql(::google::protobuf::Message& pb);
        void escapeString(std::string& str, MYSQL* conn);
        void InsertFiled(const std::string& field)
        {
            fileds_.insert(field);
        }

        static void fillMessageField(::google::protobuf::Message& pb, MysqlField* fields);
    private:
        std::string getUpdateSet(MYSQL* conn, ::google::protobuf::Message& pb);

        FieldsType fileds_;
        StringV primarykeys_;
        StringV indexes_;
        StringV unique_keys_;
        std::string foreign_keys_;
        std::string foreign_references_;
        std::string auto_increase_key_;
    };

    class DbTable
    {
    public:
        using PbTableMap = std::unordered_map<std::string, DBTable>;
        using PbPtr = std::shared_ptr<::google::protobuf::Message> ;
        using PbDbNameMap =  std::unordered_map<std::string, std::string> ;

        std::string GetCreateTableSql(::google::protobuf::Message& pb);
        std::string GetAlterTableAddFieldSql(::google::protobuf::Message& pb);
        std::string GetInsertSql(MYSQL* _mysql, ::google::protobuf::Message& pb);
        std::string GetReplaceSql(MYSQL* _mysql, ::google::protobuf::Message& pb);
        std::string GetInsertOnDupUpdateSql(MYSQL* _mysql, ::google::protobuf::Message& pb);
        std::string GetUpdateSql(MYSQL* mysql, ::google::protobuf::Message& pb);
        std::string GetUpdateSql(MYSQL* _mysql, ::google::protobuf::Message& pb, std::string where_clause);
        std::string GetSelectSql(::google::protobuf::Message& pb, const std::string& key, const std::string& val);
        std::string GetSelectSql(::google::protobuf::Message& pb, const std::string& where_clause);
        std::string GetSelectAllSql(::google::protobuf::Message& pb);
        std::string GetDeleteSql(::google::protobuf::Message& pb);
        std::string GetDeleteSql(::google::protobuf::Message& pb, std::string where_clause);
        std::string GetTruncateSql(::google::protobuf::Message& pb);

        void InsertFiled(const std::string& tbname, const std::string& field);
private:
        PbTableMap tables_;

    };

}//namespace common

#endif // !COMMON_SRC_PB2DB_DB_TABLE_H_
