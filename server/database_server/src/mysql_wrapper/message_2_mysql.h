#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <mysql.h>

#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

#include "src/type_define/type_define.h"
#include "src/mysql_wrapper/mysql_result.h"


static int32_t kPrimaryKeyIndex = 0;

void ParseFromString(::google::protobuf::Message& message, const ResultRow& row);
    
class Message2MysqlTable
{
public:
    using Fields = std::unordered_map<std::size_t, std::string>;
    using ResultRowPtr = std::unique_ptr<ResultRow>;

    Message2MysqlTable(const ::google::protobuf::Message& message_default_instance)
        : default_instance_(message_default_instance),
            descriptor_(default_instance_.GetDescriptor()),
            options_(descriptor_->options())
    {
        primarykey_field_ = descriptor_->FindFieldByName(descriptor_->field(kPrimaryKeyIndex)->name());
    }

    void set_auto_increment(uint64_t auto_increment) { auto_increment_ = auto_increment; }
    void set_db_name(const std::string& db_name) { db_name_ = db_name; }
    inline const std::string& GetTableName() { return default_instance_.GetDescriptor()->full_name(); }
    const ::google::protobuf::Message& default_instance() { return default_instance_; }

    std::string GetCreateTableSqlStmt();
    std::string GetAlterTableAddFieldSqlStmt();
    std::string GetInsertSqlStmt(const ::google::protobuf::Message& message, MYSQL* mysql);
    std::string GetSelectSqlStmt(const std::string& key, const std::string& val);
    std::string GetSelectSqlStmt( const std::string& where_clause);
    std::string GetSelectAllSqlStmt();
    std::string GetSelectAllSqlStmt(const std::string& where_clause);
    std::string GetInsertOnDupUpdateSqlStmt(const ::google::protobuf::Message& message, MYSQL* mysql);
    std::string GetInsertOnDupKeyForPrimaryKeySqlStmt(const ::google::protobuf::Message& message, MYSQL* mysql);
    std::string GetDeleteSqlStmt(const ::google::protobuf::Message& message, MYSQL* mysql);
    std::string GetDeleteSqlStmt(const std::string& where_clause, MYSQL* mysql);
    std::string GetReplaceSqlStmt(const ::google::protobuf::Message& message, MYSQL* mysql);
    std::string GetUpdateSqlStmt(const ::google::protobuf::Message& message, MYSQL* mysql);
    std::string GetUpdateSqlStmt( ::google::protobuf::Message& message, MYSQL* mysql, std::string where_clause);
    std::string GetTruncateSqlStmt(::google::protobuf::Message& message);
    std::string GetSelectColumnStmt();

    bool OnSelectTableColumnReturnSqlStmt(const MYSQL_ROW&, const unsigned long*, uint32_t);

       
private:
    std::string GetUpdateSet(const ::google::protobuf::Message& message, MYSQL* mysql);

    Fields filed_;
    StringVector primary_key_;
    StringVector indexes_;
    StringVector unique_keys_;
    std::string auto_increase_key_;
    const ::google::protobuf::Message& default_instance_;
    const ::google::protobuf::Descriptor* descriptor_{nullptr};
    const ::google::protobuf::MessageOptions&  options_;
    const ::google::protobuf::FieldDescriptor* primarykey_field_{ nullptr };
    uint64_t auto_increment_{ 0 };
    std::string db_name_;
    std::string insert_sql_stmt_;
};

class PB2DBTables
{
public:
    using PbTableMap = std::unordered_map<std::string, Message2MysqlTable>;

    void set_auto_increment(const ::google::protobuf::Message& message_default_instance, uint64_t auto_increment);
    void set_db_name(const std::string& db_name);
    std::string GetCreateTableSqlStmt(const ::google::protobuf::Message& message);
    std::string GetAlterTableAddFieldSqlStmt(const ::google::protobuf::Message& message);
    std::string GetInsertSqlStmt(::google::protobuf::Message& message);
    std::string GetReplaceSqlStmt(const::google::protobuf::Message& message);
    std::string GetInsertOnDupUpdateSqlStmt(const ::google::protobuf::Message& message);
    std::string GetInsertOnDupKeyForPrimaryKeySqlStmt(const ::google::protobuf::Message& message);
    std::string GetUpdateSqlStmt(::google::protobuf::Message& message);
    std::string GetUpdateSqlStmt(::google::protobuf::Message& message, std::string where_clause);
    std::string GetSelectSqlStmt(::google::protobuf::Message& message, const std::string& key, const std::string& val);
    std::string GetSelectSqlStmt(::google::protobuf::Message& message, const std::string& where_clause);
    std::string GetSelectAllSqlStmt(::google::protobuf::Message& message);
    std::string GetSelectAllSqlStmt(::google::protobuf::Message& message, const std::string& where_clause);
    std::string GetDeleteSqlStmt(const ::google::protobuf::Message& message);
    std::string GetDeleteSqlStmt(const ::google::protobuf::Message& message, std::string where_clause);
    std::string GetTruncateSqlStmt(::google::protobuf::Message& message);
        
    void CreateMysqlTable(const ::google::protobuf::Message& message_default_instance);

    void set_mysql(MYSQL* mysql) { mysql_ = mysql; }
    PbTableMap& tables() { return tables_; }
private:
    PbTableMap tables_;
    MYSQL* mysql_{nullptr};
    std::string db_name_;
};

