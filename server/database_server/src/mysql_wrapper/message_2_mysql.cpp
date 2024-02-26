#include "message_2_mysql.h"

#include <boost/algorithm/string.hpp>

#include "muduo/base/Logging.h"
#include "src/util/utility.h"

#include "common_proto/db_base.pb.h"


//todo 注意替换空格
void EscapeString(std::string& str, MYSQL* mysql)
{
	std::string buffer;
	// reserve space in the buffer according to the mysql documentation
	unsigned long resultSize = 0;
	buffer.resize(str.size() * 2 + 1);
	resultSize = mysql_real_escape_string(mysql, (char*)buffer.c_str(), str.c_str(), (unsigned long)str.size());
	buffer.resize(resultSize);
	str = buffer;
}

void FillMessageField(::google::protobuf::Message& message, const ResultRow& row)
{
    const auto reflection = message.GetReflection();
    const auto descriptor = message.GetDescriptor();
    for (int32_t i = 0; i < descriptor->field_count(); ++i)
    {
        const ::google::protobuf::FieldDescriptor* field_desc = descriptor->FindFieldByName(descriptor->field(i)->name());
        auto filed = descriptor->field(i);
        switch (filed->cpp_type())
        {
        case ::google::protobuf::FieldDescriptor::CPPTYPE_INT32:
        {
            int32_t type_value = StringToNumber<int32_t>(row[i]);
            reflection->SetInt32(&message, field_desc, type_value);
        }
        break;
        case ::google::protobuf::FieldDescriptor::CPPTYPE_INT64:
        {
            int64_t type_value = StringToNumber<int64_t>(row[i]);
            reflection->SetInt64(&message, field_desc, type_value);
        }
        break;
        case  ::google::protobuf::FieldDescriptor::CPPTYPE_UINT32:
        {
            uint32_t type_value = StringToNumber<uint32_t>(row[i]);
            reflection->SetUInt32(&message, field_desc, type_value);
        }
        break;
        case  ::google::protobuf::FieldDescriptor::CPPTYPE_UINT64:
        {
            uint64_t type_value = StringToNumber<uint64_t>(row[i]);
            reflection->SetUInt64(&message, field_desc, type_value);
        }
        break;
        case ::google::protobuf::FieldDescriptor::CPPTYPE_FLOAT:
        {
            float type_value = StringToNumber<float>(row[i]);
            reflection->SetFloat(&message, field_desc, type_value);
        }
        break;
        case ::google::protobuf::FieldDescriptor::CPPTYPE_STRING:
        {
            if (row[i] == nullptr)
            {
                std::string type_value = "";
                reflection->SetString(&message, field_desc, type_value);
            }
            else
            {
                std::string type_value(row[i]);
                reflection->SetString(&message, field_desc, type_value);
            }
        }
        break;
        case  ::google::protobuf::FieldDescriptor::CPPTYPE_DOUBLE:
        {
            double type_value = StringToNumber<double>(row[i]);
            reflection->SetDouble(&message, field_desc, type_value);
        }
        break;
        case  ::google::protobuf::FieldDescriptor::CPPTYPE_BOOL:
        {
            auto length = row.length(i);
            if (length != 0 && row[i] != nullptr)
            {
                bool typeValue = StringToNumber<bool>(row[i]);
                reflection->SetBool(&message, field_desc, typeValue);
            }
            else
            {
                reflection->SetBool(&message, field_desc, false);
            }

        }
        break;
        case  ::google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE:
        {
            auto length = row.length(i);
            if (length != 0 && row[i] != NULL)
            {
                google::protobuf::Message* sub_message = reflection->MutableMessage(&message, field_desc);//不会为null;
                sub_message->ParsePartialFromArray(row[i], length);
            }
        }
        break;
        default:

            return;
        }
    }
}

const char* table_name_descriptor[google::protobuf::FieldDescriptor::MAX_CPPTYPE + 1] =
{
    "",
    "int NOT NULL  ",
    "bigint NOT NULL  ",
    "int unsigned NOT NULL  ",
    "bigint unsigned NOT NULL  ",
    "double NOT NULL DEFAULT '0'",
    "float NOT NULL DEFAULT '0'",
    "bool ",
    "int NOT NULL  ",
    "varchar(256)",
    "Blob",
};

#undef  GetMessage
std::string SerializeFieldAsString(const ::google::protobuf::Message& message, const google::protobuf::FieldDescriptor* fieldDesc, MYSQL* mysql)
{
    const auto reflect = message.GetReflection();
    std::string field_value;
    switch (fieldDesc->cpp_type())
    {
    case google::protobuf::FieldDescriptor::CPPTYPE_INT32:
        field_value = std::to_string(reflect->GetInt32(message, fieldDesc));
        break;
    case google::protobuf::FieldDescriptor::CPPTYPE_UINT32:
        field_value = std::to_string(reflect->GetUInt32(message, fieldDesc));
        break;
    case google::protobuf::FieldDescriptor::CPPTYPE_FLOAT:
        field_value = std::to_string(reflect->GetFloat(message, fieldDesc));
        break;
    case google::protobuf::FieldDescriptor::CPPTYPE_STRING:
        field_value = reflect->GetString(message, fieldDesc);
        break;
    case google::protobuf::FieldDescriptor::CPPTYPE_INT64:
        field_value = std::to_string(reflect->GetInt64(message, fieldDesc));
        break;
    case  google::protobuf::FieldDescriptor::CPPTYPE_UINT64:
        field_value = std::to_string(reflect->GetUInt64(message, fieldDesc));
        break;
    case  google::protobuf::FieldDescriptor::CPPTYPE_DOUBLE:
        field_value = std::to_string(reflect->GetDouble(message, fieldDesc));
        break;
    case  google::protobuf::FieldDescriptor::CPPTYPE_BOOL:
        field_value = std::to_string(reflect->GetBool(message, fieldDesc));
        break;
    case google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE://message结构的类型,有这个结构再序列化，没有的话就是空串;
        field_value = (reflect->HasField(message, fieldDesc)) ? (reflect->GetMessage(message, fieldDesc)).SerializeAsString() : "";
        break;
    default:
        return field_value;
    }
    EscapeString(field_value, mysql);
    return field_value;
}

std::string Message2MysqlSql::GetCreateTableSql()
{
    std::string sql = "CREATE TABLE IF NOT EXISTS " + GetTypeName();
    if (options_.HasExtension(OptionPrimaryKey))
    {
        boost::split(primary_key_, options_.GetExtension(OptionPrimaryKey), boost::is_any_of(","));
    }
    if (options_.HasExtension(OptionIndex))
    {
        boost::split(indexes_, options_.GetExtension(OptionIndex), boost::is_any_of(","));
    }
    if (options_.HasExtension(OptionUniqueKey))
    {
        boost::split(unique_keys_, options_.GetExtension(OptionUniqueKey), boost::is_any_of(","));
    }
    auto_increase_key_ = options_.GetExtension(OptionAutoIncrementKey);
    foreign_keys_ = options_.GetExtension(OptionForeignKey);
    foreign_references_ = options_.GetExtension(OptionForeignReferences);

    sql += " (";
    bool need_comma = false;
    for (int32_t i = 0; i < descriptor_->field_count(); ++i)
    {
        auto filed = descriptor_->field(i);
        if (need_comma == true)
        {
            sql += ", ";
        }
        else
        {
            need_comma = true;
        }
        sql += filed->name();

        sql += " ";
        sql += table_name_descriptor[filed->cpp_type()];
        if (i == kPrimaryKeyIndex)
        {
            sql += " NOT NULL ";
        }
        if (filed->name() == auto_increase_key_)
        {
            sql += " AUTO_INCREMENT ";
        }
    }

    sql += ", PRIMARY KEY (";
    sql += options_.GetExtension(OptionPrimaryKey);
    sql += ")";
    if (!foreign_keys_.empty() && !foreign_references_.empty())
    {
        sql += ", FOREIGN KEY (";
        sql += foreign_keys_;
        sql += ")";
        sql += "REFERENCES ";
        sql += foreign_references_;
    }
    if (options_.GetExtension(OptionUniqueKey).length() > 0)
    {
        sql += ", UNIQUE KEY (";
        sql += options_.GetExtension(OptionUniqueKey);
        sql += ")";
    }
    for (const auto& it : indexes_)
    {
        sql += ",INDEX (";
        sql += it;
        sql += ")";
    }
    sql += ") ENGINE = INNODB ";
    if (!auto_increase_key_.empty())
    {
        sql += "AUTO_INCREMENT=1";
    }

    return sql;
}

std::string Message2MysqlSql::GetAlterTableAddFieldSql()
{
    if ((std::size_t)descriptor_->field_count() == filed_.size())
    {
        return "";
    }
    std::string sql("ALTER TABLE ");
    sql += GetTypeName();
    for (int32_t i = 0; i < descriptor_->field_count(); ++i)
    {
        auto filed = descriptor_->field(i);
        auto fi = filed_.find(i);
        if (fi != filed_.end())
        {
            continue;
        }
        sql += " ADD COLUMN ";
        sql += filed->name();
        sql += " ";
        sql += table_name_descriptor[filed->cpp_type()];
        if (i + 1 < descriptor_->field_count())
        {
            sql += ",";
        }        
    }
    
    if (nullptr == primarykey_field_)
    {
        std::string es("Table ");
        es += GetTypeName();
        es += " Has not PrimaryKey!";
        LOG_FATAL << es;
        return "";
    }
    sql += ';';
    return sql;
}

std::string Message2MysqlSql::GetInsertSql(const ::google::protobuf::Message& message, MYSQL* mysql)
{
    std::string sql = "INSERT INTO " + GetTypeName();
    sql += " (";
    bool bNeedComma = false;
    const ::google::protobuf::FieldDescriptor* field_desc = nullptr;
    for (int32_t i = 0; i < descriptor_->field_count(); ++i)
    {
        if (bNeedComma == true)
        {
            sql += ", ";
        }
        else
        {
            bNeedComma = true;
        }

        sql += descriptor_->field(i)->name();
    }

    sql += ") VALUES (";

    bNeedComma = false;
    for (int32_t i = 0; i < descriptor_->field_count(); ++i)
    {

        if (bNeedComma == true)
        {
            sql += ", ";
        }
        else
        {
            bNeedComma = true;
        }

        field_desc = descriptor_->FindFieldByName(descriptor_->field(i)->name());
        std::string value = SerializeFieldAsString(message, field_desc, mysql);
        
        sql += "'";
        sql += value;
        sql += "'";
    }
    sql += ")";
    return sql;
}

std::string Message2MysqlSql::GetInsertOnDupUpdateSql(const ::google::protobuf::Message& message, MYSQL* mysql)
{
    std::string sql = GetInsertSql(message, mysql);
    sql += " ON DUPLICATE KEY UPDATE ";
    sql += GetUpdateSet(message, mysql);
    return sql;
}

std::string Message2MysqlSql::GetInsertOnDupKeyForPrimaryKey(const ::google::protobuf::Message& message, MYSQL* mysql)
{
    std::string sql = GetInsertSql(message, mysql);
    sql += " ON DUPLICATE KEY UPDATE ";
    sql += " " + primarykey_field_->name();
    std::string value = SerializeFieldAsString(message, primarykey_field_, mysql);
    sql += "=";
    sql += "'";
    sql += value;
    sql += "';";
    return sql;
}


bool Message2MysqlSql::OnSelectTableColumnReturn(const MYSQL_ROW& ptr, const unsigned long* filed_length, uint32_t filed_size)
{
    for (std::size_t i = 0; i < filed_size; ++i)
    {
        std::string field_name(ptr[i], filed_length[i]);
        filed_.emplace(filed_.size(), field_name);
    }
    return true;
}

std::string Message2MysqlSql::GetSelectSql(const std::string& key, const std::string& val)
{
    std::string sql = "select ";
    bool bNeedComma = false;
    for (int32_t i = 0; i < descriptor_->field_count(); ++i)
    {
        if (bNeedComma == true)
        {
            sql += ", ";
        }
        else
        {
            bNeedComma = true;
        }

        sql += " " + descriptor_->field(i)->name();
    }

    sql += " from ";
    sql += GetTypeName();
    sql += " where ";
    sql += key;
    sql += " = '";
    sql += val;
    sql += "';";
    return sql;
}

std::string Message2MysqlSql::GetSelectSql(const std::string& where_clause)
{
    std::string sql = "select ";
    bool bNeedComma = false;
    for (int32_t i = 0; i < descriptor_->field_count(); ++i)
    {
        if (bNeedComma == true)
        {
            sql += ", ";
        }
        else
        {
            bNeedComma = true;
        }

        sql += " " + descriptor_->field(i)->name();
    }
    sql += " from ";
    sql += GetTypeName();
    sql += " where ";
    sql += where_clause;
    sql += ";";
    return sql;
}

std::string Message2MysqlSql::GetSelectAllSql()
{
    std::string sql = "select ";
    bool bNeedComma = false;
    for (int32_t i = 0; i < descriptor_->field_count(); ++i)
    {
        if (bNeedComma == true)
        {
            sql += ", ";
        }
        else
        {
            bNeedComma = true;
        }

        sql += " " + descriptor_->field(i)->name();
    }
    sql += " from ";
    sql += GetTypeName();
    return sql;
}


std::string Message2MysqlSql::GetSelectAllSql(const std::string& where_clause)
{
    std::string sql = "select ";
    bool bNeedComma = false;
    for (int32_t i = 0; i < descriptor_->field_count(); ++i)
    {
        if (bNeedComma == true)
        {
            sql += ", ";
        }
        else
        {
            bNeedComma = true;
        }

        sql += " " + descriptor_->field(i)->name();
    }
    sql += " from ";
    sql += GetTypeName();
    sql += " where ";
    sql += where_clause;
    sql += ";";
    return sql;
}

std::string Message2MysqlSql::GetDeleteSql(const ::google::protobuf::Message& message, MYSQL* mysql)
{
    std::string sql = "delete ";
    sql += " from ";
    sql += GetTypeName();
    sql += " where ";
    sql += descriptor_->field(kPrimaryKeyIndex)->name();
    std::string value = SerializeFieldAsString(message, primarykey_field_, mysql);
    sql += " = '";
    sql += value;
    sql += "'";
    return sql;
}

std::string Message2MysqlSql::GetDeleteSql( const std::string& where_clause, MYSQL* mysql)
{
    std::string sql = "delete ";
    sql += " from ";
    sql += GetTypeName();
    sql += " where ";
    sql += where_clause;
    return sql;
}

std::string Message2MysqlSql::GetReplaceSql(const ::google::protobuf::Message& message, MYSQL* mysql)
{
    std::string sql = "REPLACE INTO " + message.GetTypeName();
    sql += " (";
    bool bNeedComma = false;
    const ::google::protobuf::FieldDescriptor* file_desc = nullptr;
    for (int32_t i = 0; i < descriptor_->field_count(); ++i)
    {
        if (bNeedComma == true)
        {
            sql += ", ";
        }
        else
        {
            bNeedComma = true;
        }
        sql += descriptor_->field(i)->name();
    }
    sql += ") VALUES (";
    bNeedComma = false;
    for (int32_t i = 0; i < descriptor_->field_count(); ++i)
    {
        if (bNeedComma == true)
        {
            sql += ", ";
        }
        else
        {
            bNeedComma = true;
        }
        file_desc = descriptor_->FindFieldByName(descriptor_->field(i)->name());
        std::string value = SerializeFieldAsString(message, file_desc, mysql);
        sql += "'";
        sql += value;
        sql += "'";
    }
    sql += ")";
    return sql;
}

std::string Message2MysqlSql::GetUpdateSet(const ::google::protobuf::Message& message, MYSQL* mysql)
{
    std::string sql;
    const ::google::protobuf::FieldDescriptor* file_desc = nullptr;
    bool bNeedComma = false;
    auto pdesc_ = message.GetDescriptor();
    const google::protobuf::Reflection* reflect = message.GetReflection();
    int field_count = pdesc_->field_count();
    for (int32_t i = 0; i < field_count; ++i)
    {
        const::std::string& field_name = pdesc_->field(i)->name();
        file_desc = pdesc_->FindFieldByName(field_name);
        bool hasField = reflect->HasField(message, file_desc);
        if (hasField)
        {
            if (bNeedComma == true)
            {
                sql += ", ";
            }
            else
            {
                bNeedComma = true;
            }
            sql += " " + field_name;
            std::string value = SerializeFieldAsString(message, file_desc, mysql);
            sql += "=";
            sql += "'";
            sql += value;
            sql += "'";
        }
    }
    return sql;
}

std::string Message2MysqlSql::GetUpdateSql(const ::google::protobuf::Message& message, MYSQL* mysql)
{
    const ::google::protobuf::FieldDescriptor* pFileDesc = nullptr;
    std::string sql = "UPDATE " + message.GetTypeName();
    bool bNeedComma = false;
    sql += " SET ";
    sql += GetUpdateSet(message, mysql);
    sql += " where ";
    bNeedComma = false;
    for (auto& strPrimary : primary_key_)
    {
        pFileDesc = message.GetDescriptor()->FindFieldByName(strPrimary);
        const google::protobuf::Reflection* reflect = message.GetReflection();
        bool hasField = reflect->HasField(message, pFileDesc);
        if (hasField)
        {
            if (bNeedComma == true)
            {
                sql += " and ";
            }
            else
            {
                bNeedComma = true;
            }
            sql += strPrimary;
            std::string value = SerializeFieldAsString(message, pFileDesc, mysql);
            sql += "='";
            sql += value;
            sql += "'";
        }
    }
    return sql;
}

std::string Message2MysqlSql::GetUpdateSql(::google::protobuf::Message& message, MYSQL* mysql, std::string where_clause)
{
    const ::google::protobuf::FieldDescriptor* file_desc = nullptr;

    std::string sql = "UPDATE " + GetTypeName();
    bool bNeedComma = false;
    sql += " SET ";
    for (int32_t i = 0; i < descriptor_->field_count(); ++i)
    {
        if (bNeedComma == true)
        {
            sql += ", ";
        }
        else
        {
            bNeedComma = true;
        }
        sql += " " + descriptor_->field(i)->name();
        file_desc = descriptor_->FindFieldByName(descriptor_->field(i)->name());
        std::string value = SerializeFieldAsString(message, file_desc, mysql);
        sql += "=";

        sql += "'";
        sql += value;
        sql += "'";
    }

    if (!where_clause.empty())
    {
        sql += " where ";
        sql += where_clause;
    }
    else
    {
        sql = "";
    }
    return sql;
}

std::string Message2MysqlSql::GetTruncateSql(::google::protobuf::Message& message)
{
    std::string sql = "Truncate " + message.GetDescriptor()->full_name();
    return sql;
}

std::string Message2MysqlSql::GetSelectColumn()
{
    return std::string("SELECT COLUMN_NAME FROM INFORMATION_NAME.COLUMNS  WHERE  TABLE_NAME = '") + 
        GetTypeName() + 
        std::string("';");
}

void Pb2DbTables::set_auto_increment(const ::google::protobuf::Message& message, uint64_t auto_increment)
{
    const auto& table_name = message.GetDescriptor()->full_name();
    auto it = tables_.find(table_name);
    if (it == tables_.end())
    {
        return;
    }
    it->second.set_auto_increment(auto_increment);
}

std::string Pb2DbTables::GetCreateTableSql(const ::google::protobuf::Message& message)
{
    const auto& table_name = message.GetDescriptor()->full_name();
    auto it = tables_.find(table_name);
    if (it == tables_.end())
    {
        return "";
    }
    return it->second.GetCreateTableSql();
}

std::string Pb2DbTables::GetAlterTableAddFieldSql(const ::google::protobuf::Message& message)
{
    const auto& table_name = message.GetDescriptor()->full_name();
    auto it = tables_.find(table_name);
    if (it == tables_.end())
    {
        return "";
    }
    return it->second.GetAlterTableAddFieldSql();
}

void Pb2DbTables::CreateMysqlTable(const ::google::protobuf::Message& message_default_instance)
{
    tables_.emplace(message_default_instance.GetTypeName(), Message2MysqlSql(message_default_instance)); 
}

std::string Pb2DbTables::GetInsertSql(::google::protobuf::Message& message)
{
    const auto& table_name = message.GetDescriptor()->full_name();
    auto it = tables_.find(table_name);
    if (it == tables_.end())
    {
        return "";
    }
    return it->second.GetInsertSql(message, mysql_);
}

std::string Pb2DbTables::GetReplaceSql(const ::google::protobuf::Message& message)
{
    const auto& table_name = message.GetDescriptor()->full_name();
    auto it = tables_.find(table_name);
    if (it == tables_.end())
    {
        return "";
    }
    return it->second.GetReplaceSql(message, mysql_);
}

std::string Pb2DbTables::GetInsertOnDupUpdateSql(const ::google::protobuf::Message& message)
{
    const auto& table_name = message.GetDescriptor()->full_name();
    auto it = tables_.find(table_name);
    if (it == tables_.end())
    {
        return "";
    }
    return it->second.GetInsertOnDupUpdateSql(message, mysql_);
}

std::string Pb2DbTables::GetInsertOnDupKeyForPrimaryKey(const ::google::protobuf::Message& message)
{
    const auto& table_name = message.GetDescriptor()->full_name();
    auto it = tables_.find(table_name);
    if (it == tables_.end())
    {
        return "";
    }
    return it->second.GetInsertOnDupKeyForPrimaryKey(message, mysql_);
}

std::string Pb2DbTables::GetUpdateSql(::google::protobuf::Message& message)
{
    const auto& table_name = message.GetDescriptor()->full_name();
    auto it = tables_.find(table_name);
    if (it == tables_.end())
    {
        return "";
    }
    return it->second.GetUpdateSql(message, mysql_);
}

std::string Pb2DbTables::GetUpdateSql(::google::protobuf::Message& message,  std::string where_clause)
{
    const auto& table_name = message.GetDescriptor()->full_name();
    auto it = tables_.find(table_name);
    if (it == tables_.end())
    {
        return "";
    }
    return it->second.GetUpdateSql(message, mysql_, where_clause);
}

std::string Pb2DbTables::GetSelectSql(::google::protobuf::Message& message, const std::string& key, const std::string& val)
{
    const auto& table_name = message.GetDescriptor()->full_name();
    auto it = tables_.find(table_name);
    if (it == tables_.end())
    {
        return "";
    }
    return it->second.GetSelectSql(key, val);
}

std::string Pb2DbTables::GetSelectSql(::google::protobuf::Message& message, const std::string& where_clause)
{
    const auto& table_name = message.GetDescriptor()->full_name();
    auto it = tables_.find(table_name);
    if (it == tables_.end())
    {
        return "";
    }
    return it->second.GetSelectSql(where_clause);
}

std::string Pb2DbTables::GetSelectAllSql(::google::protobuf::Message& message)
{
    const auto& table_name = message.GetDescriptor()->full_name();
    auto it = tables_.find(table_name);
    if (it == tables_.end())
    {
        return "";
    }
    return it->second.GetSelectAllSql();
}


std::string Pb2DbTables::GetSelectAllSql(::google::protobuf::Message& message, const std::string& where_clause)
{
    const auto& table_name = message.GetDescriptor()->full_name();
    auto it = tables_.find(table_name);
    if (it == tables_.end())
    {
        return "";
    }
    return it->second.GetSelectAllSql(where_clause);
}

std::string Pb2DbTables::GetDeleteSql(const ::google::protobuf::Message& message)
{
    const auto& table_name = message.GetDescriptor()->full_name();
    auto it = tables_.find(table_name);
    if (it == tables_.end())
    {
        return "";
    }
    return it->second.GetDeleteSql(message, mysql_);
}

std::string Pb2DbTables::GetDeleteSql(const ::google::protobuf::Message& message, std::string where_clause)
{
    const auto& table_name = message.GetDescriptor()->full_name();
    auto it = tables_.find(table_name);
    if (it == tables_.end())
    {
        return "";
    }
    return it->second.GetDeleteSql(where_clause, mysql_);
}

std::string Pb2DbTables::GetTruncateSql(::google::protobuf::Message& message)
{
    const auto& table_name = message.GetDescriptor()->full_name();
    auto it = tables_.find(table_name);
    if (it == tables_.end())
    {
        return "";
    }
    return it->second.GetTruncateSql(message);
}
