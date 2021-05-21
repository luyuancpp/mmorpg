#include "dbtable.h"

#include <sstream>
#include <boost/algorithm/string.hpp>

//#include "playerdb.pb.h"
#include "GameDB.pb.h"
#include "Logger.h"
#include "../Common/Game/ConfigJson.h"
#include "Snowflake.h"


std::string DBTableMng::m_inc_base;

const char* MysqlTypeName[google::protobuf::FieldDescriptor::MAX_CPPTYPE + 1] =
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

static int32_t nPrimaryKeyIndex = 0;

 #undef  GetMessage
std::string convertFieldValue(const ::google::protobuf::Message &message, const google::protobuf::FieldDescriptor*  fieldDesc)
{
	const google::protobuf::Reflection * reflect = message.GetReflection();
	std::string fieldValue;
	switch (fieldDesc->cpp_type())
	{
	case google::protobuf::FieldDescriptor::CPPTYPE_INT32:
		fieldValue = std::to_string(reflect->GetInt32(message, fieldDesc));
		break;
	case google::protobuf::FieldDescriptor::CPPTYPE_UINT32:
		fieldValue = std::to_string(reflect->GetUInt32(message, fieldDesc));
		break;
	case google::protobuf::FieldDescriptor::CPPTYPE_FLOAT:
		fieldValue = std::to_string(reflect->GetFloat(message, fieldDesc));
		break;
	case google::protobuf::FieldDescriptor::CPPTYPE_STRING:
		fieldValue = reflect->GetString(message, fieldDesc);
		break;
	case google::protobuf::FieldDescriptor::CPPTYPE_INT64:
		fieldValue = std::to_string(reflect->GetInt64(message, fieldDesc));
		break;
	case  google::protobuf::FieldDescriptor::CPPTYPE_UINT64:
		fieldValue = std::to_string(reflect->GetUInt64(message, fieldDesc));
		break;
	case  google::protobuf::FieldDescriptor::CPPTYPE_DOUBLE:
		fieldValue = std::to_string(reflect->GetDouble(message, fieldDesc));
		break;
	case  google::protobuf::FieldDescriptor::CPPTYPE_BOOL:
		fieldValue = std::to_string(reflect->GetBool(message, fieldDesc));
		break;
	case google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE://message结构的类型,有这个结构再序列化，没有的话就是空串;
		fieldValue = (reflect->HasField(message, fieldDesc)) ? (reflect->GetMessage(message, fieldDesc)).SerializeAsString() : "";
		break;
	default:
		return fieldValue;
	}
	return fieldValue;
}

#define  GetMessage

void DBTable::fillMessageField(::google::protobuf::Message &pb, MysqlField * fields)
{
	const ::google::protobuf::Reflection * reflection = pb.GetReflection();
	const ::google::protobuf::Descriptor * pDesc = pb.GetDescriptor();

	for (int32_t i = 0; i < pb.GetDescriptor()->field_count(); ++i)
	{
		const ::google::protobuf::FieldDescriptor* fieldDesc = pDesc->FindFieldByName(pDesc->field(i)->name());

		switch (pDesc->field(i)->cpp_type())
		{
		case ::google::protobuf::FieldDescriptor::CPPTYPE_INT32:
		{
				int32_t typeValue = stringToNum<int32_t>(fields[i].field_chars);
				reflection->SetInt32(&pb, fieldDesc, typeValue);
		}
			break;
		case ::google::protobuf::FieldDescriptor::CPPTYPE_INT64:
		{
                int64_t typeValue = stringToNum<int64_t>(fields[i].field_chars);
				reflection->SetInt64(&pb, fieldDesc, typeValue);
		}
			break;
		case  ::google::protobuf::FieldDescriptor::CPPTYPE_UINT32:
		{
				uint32_t typeValue = stringToNum<uint32_t>(fields[i].field_chars);
				reflection->SetUInt32(&pb, fieldDesc, typeValue);
		}
			break;
		case  ::google::protobuf::FieldDescriptor::CPPTYPE_UINT64:
		{
				uint64_t typeValue = stringToNum<uint64_t>(fields[i].field_chars);
				reflection->SetUInt64(&pb, fieldDesc, typeValue);
		}
			break;
		case ::google::protobuf::FieldDescriptor::CPPTYPE_FLOAT:
		{
				float typeValue = stringToNum<float>(fields[i].field_chars);
				reflection->SetFloat(&pb, fieldDesc, typeValue);
		}
			break;
		case ::google::protobuf::FieldDescriptor::CPPTYPE_STRING:
		{
			if (fields[i].field_chars == nullptr)
			{
				std::string typeValue = "";
				reflection->SetString(&pb, fieldDesc, typeValue);
			}
			else
			{
				std::string typeValue(fields[i].field_chars);
				reflection->SetString(&pb, fieldDesc, typeValue);
			}
				
		}
			break;
		case  ::google::protobuf::FieldDescriptor::CPPTYPE_DOUBLE:
		{
				double typeValue = stringToNum<double>(fields[i].field_chars);
				reflection->SetDouble(&pb, fieldDesc, typeValue);
		}
			break;
		case  ::google::protobuf::FieldDescriptor::CPPTYPE_BOOL:
		{
			if (fields[i].field_len != 0 && fields[i].field_chars != nullptr)
			{
				bool typeValue = stringToNum<bool>(fields[i].field_chars);
				reflection->SetBool(&pb, fieldDesc, typeValue);
			}
			else
			{
				reflection->SetBool(&pb, fieldDesc, false);
			}
				
		}
			break;
		case  ::google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE:
		{
            if (fields[i].field_len != 0 && fields[i].field_chars != NULL)
            {
                google::protobuf::Message* subMessage = reflection->MutableMessage(&pb, fieldDesc);//不会为null;
                subMessage->ParseFromArray(fields[i].field_chars, fields[i].field_len);
            }
		}
			break;
		default:
			
			return;
		}
	}
	
	
}


DBTable::DBTable()
{

}

DBTable::~DBTable()
{

}

std::string DBTable::GetCreateTableSql(::google::protobuf::Message &pb)
{

	std::string sql = "CREATE TABLE IF NOT EXISTS " + pb.GetTypeName();

	const ::google::protobuf::MessageOptions & options = pb.GetDescriptor()->options();
	m_sPrimaryKeys.clear();
    m_sIndexes.clear();
    m_sAutoIncreaseKey.clear();
    m_sForeignKeys.clear();
    m_sForeignReferences.clear();
    m_vUniqueKeys.clear();
    if (options.HasExtension(OptionPrimaryKey))
    {
        boost::split(m_sPrimaryKeys, options.GetExtension(OptionPrimaryKey), boost::is_any_of(","));
    }
    if (options.HasExtension(OptionIndex))
    {
        boost::split(m_sIndexes, options.GetExtension(OptionIndex), boost::is_any_of(","));
    }
    if (options.HasExtension(OptionUniqueKey))
    {
        boost::split(m_vUniqueKeys, options.GetExtension(OptionUniqueKey), boost::is_any_of(","));
    }
    m_sAutoIncreaseKey = options.GetExtension(OptionAutoIncrementKey);
    m_sForeignKeys = options.GetExtension(OptionForeignKey);
    m_sForeignReferences = options.GetExtension(OptionForeignReferences);

	sql += " (";
	bool bNeedComma = false;
	for (int32_t i = 0; i < pb.GetDescriptor()->field_count(); ++i)
	{
		if (bNeedComma == true)
		{
			sql += ", ";
		}
		else
		{
			bNeedComma = true;
		}
		sql += pb.GetDescriptor()->field(i)->name();
		
		sql += " ";
		sql += MysqlTypeName[pb.GetDescriptor()->field(i)->cpp_type()];
		if (i == nPrimaryKeyIndex)
		{
			sql += " NOT NULL ";
		}
        if (pb.GetDescriptor()->field(i)->name() == m_sAutoIncreaseKey)
        {
            sql += " AUTO_INCREMENT ";
        }
	}

	sql += ", PRIMARY KEY (";
	sql += options.GetExtension(OptionPrimaryKey);
    sql += ")";
    if (!m_sForeignKeys.empty() && !m_sForeignReferences.empty())
    {
        sql += ", FOREIGN KEY (";
        sql += m_sForeignKeys;
        sql += ")";
        sql += "REFERENCES ";
        sql += m_sForeignReferences;
    }
    if (options.GetExtension(OptionUniqueKey).length() > 0)
    {
        sql += ", UNIQUE KEY (";
        sql += options.GetExtension(OptionUniqueKey);
        sql += ")";
    }

    for (const auto &it : m_sIndexes)
    {
        sql += ",INDEX (";
        sql += it;
        sql += ")";
    }
    sql += ") ENGINE = INNODB ";
	if (sql.find("AUTO_INCREMENT") != std::string::npos)
	{
		if ( pb.GetTypeName() == "user")
		{
			//sql += "AUTO_INCREMENT = " + g_config_json.m_root["AUTOINC"][pb.GetTypeName()].asString();
			sql += "AUTO_INCREMENT = " + DBTableMng::m_inc_base;
		}
	}
	return sql;
}

std::string DBTable::GetAlterTableAddFieldSql(::google::protobuf::Message &pb)
{
	std::string sql;

	bool bNeedComma = false;
	bool bNeedAlter = false;
	sql += "ALTER TABLE " + pb.GetTypeName();

	for (int32_t i = 0; i < pb.GetDescriptor()->field_count(); ++i)
	{
		if (m_vFieldList.find(pb.GetDescriptor()->field(i)->name()) != m_vFieldList.end())
		{
			continue;
		}

		bNeedAlter = true;
		sql += " ADD COLUMN ";
		sql += pb.GetDescriptor()->field(i)->name();
		sql += " ";
		sql += MysqlTypeName[pb.GetDescriptor()->field(i)->cpp_type()];
		sql += ",";
	}
	sql[sql.length()-1] = ';';
	if (bNeedAlter)
	{
		return sql;
	}
	else
	{
		return "";
	}
	
}



std::string DBTable::GetInsertSql(MYSQL * conn, ::google::protobuf::Message &pb)
{
	std::string sql = "INSERT INTO " + pb.GetTypeName();
	sql += " (";
	bool bNeedComma = false;
	const ::google::protobuf::Descriptor* pDesc = pb.GetDescriptor();
	const ::google::protobuf::FieldDescriptor * pFileDesc = NULL;
	for (int32_t i = 0; i < pDesc->field_count(); ++i)
	{
	
		if (bNeedComma == true)
		{
			sql += ", ";
		}
		else
		{
			bNeedComma = true;
		}



		sql += pDesc->field(i)->name();
		
	}

	sql += ") VALUES (";

	bNeedComma = false;
	for (int32_t i = 0; i < pDesc->field_count(); ++i)
	{
	
		if (bNeedComma == true)
		{
			sql += ", ";
		}
		else
		{
			bNeedComma = true;
		}

		
		pFileDesc = pDesc->FindFieldByName(pDesc->field(i)->name());
		std::string value = convertFieldValue(pb, pFileDesc);
        escapeString(value, conn);
		sql += "'";
		sql += value;
		sql += "'";
	}
	sql += ")";
	return sql;
}

std::string DBTable::GetInsertOnDupUpdateSql(MYSQL *conn, ::google::protobuf::Message &pb)
{
    std::string sql = GetInsertSql(conn, pb);
    sql += " ON DUPLICATE KEY UPDATE ";
    sql += getUpdateSet(conn, pb);
    return sql;
}

void DBTable::escapeString(std::string &str, MYSQL* conn)
{
    std::string buffer;
    // reserve space in the buffer according to the mysql documentation
    unsigned long resultSize = 0;
    buffer.resize(str.size() * 2 + 1);
    try
    {
        resultSize = mysql_real_escape_string(conn, (char*)buffer.c_str(), str.c_str(), (unsigned long)str.size());
    }
    catch (std::exception& e)
    {
        ELOG("mysql_real_escape_string exception:%s", e.what());
    }
    // now, resize to the real size
    buffer.resize(resultSize);

    // job's done
    str = buffer;
}

std::string DBTable::GetSelectSql(::google::protobuf::Message &pb, const std::string & key, const std::string &val)
{
	const ::google::protobuf::FieldDescriptor * pFileDesc = pb.GetDescriptor()->FindFieldByName(pb.GetDescriptor()->field(nPrimaryKeyIndex)->name());
	if (NULL == pFileDesc)
	{
		return "";
	}

	std::string sql = "select ";
	bool bNeedComma = false;
	for (int32_t i = 0; i < pb.GetDescriptor()->field_count(); ++i)
	{
		if (bNeedComma == true)
		{
			sql += ", ";
		}
		else
		{
			bNeedComma = true;
		}

		sql += " " + pb.GetDescriptor()->field(i)->name();
	}
	
	sql += " from ";
	sql += pb.GetTypeName();
	if (!key.empty() && !val.empty())
	{
		sql += " where ";
		sql += key;
		sql += " = '";
		sql += val;
		sql += "'";
	}
	
	return sql;
}

std::string DBTable::GetSelectSql(::google::protobuf::Message &pb, const std::string & where_clause)
{
    const ::google::protobuf::FieldDescriptor * pFileDesc = pb.GetDescriptor()->FindFieldByName(pb.GetDescriptor()->field(nPrimaryKeyIndex)->name());
    if (NULL == pFileDesc)
    {
        return "";
    }

    std::string sql = "select ";
    bool bNeedComma = false;
    for (int32_t i = 0; i < pb.GetDescriptor()->field_count(); ++i)
    {
        if (bNeedComma == true)
        {
            sql += ", ";
        }
        else
        {
            bNeedComma = true;
        }

        sql += " " + pb.GetDescriptor()->field(i)->name();
    }

    sql += " from ";
    sql += pb.GetTypeName();
    if (!where_clause.empty())
    {
        sql += " where ";
        sql += where_clause;
    }
    else
    {
        ELOG("where_clause is empty!");
        sql = "";
    }

    return sql;
}

std::string DBTable::GetSelectAllSql(::google::protobuf::Message &pb)
{
	const ::google::protobuf::FieldDescriptor * pFileDesc = pb.GetDescriptor()->FindFieldByName(pb.GetDescriptor()->field(nPrimaryKeyIndex)->name());
	if (NULL == pFileDesc)
	{
		return "";
	}

	std::string sql = "select ";
	bool bNeedComma = false;
	for (int32_t i = 0; i < pb.GetDescriptor()->field_count(); ++i)
	{
		if (bNeedComma == true)
		{
			sql += ", ";
		}
		else
		{
			bNeedComma = true;
		}

		sql += " " + pb.GetDescriptor()->field(i)->name();
	}

	sql += " from ";
	sql += pb.GetTypeName();
	return sql;
}

std::string DBTable::GetDeleteSql(::google::protobuf::Message &pb)
{
	const ::google::protobuf::FieldDescriptor * pFileDesc = pb.GetDescriptor()->FindFieldByName(pb.GetDescriptor()->field(nPrimaryKeyIndex)->name());
	if (NULL == pFileDesc)
	{
		return "";
	}

	std::string sql = "delete ";

	sql += " from ";
	sql += pb.GetTypeName();
	sql += " where ";
	sql += pb.GetDescriptor()->field(nPrimaryKeyIndex)->name();
	std::string value = convertFieldValue(pb, pFileDesc);
	sql += " = '";
	sql += value;
	sql += "'";;
	return sql;
}

std::string DBTable::GetDeleteSql(::google::protobuf::Message &pb, const std::string& where_clause)
{
    const ::google::protobuf::FieldDescriptor * pFileDesc = pb.GetDescriptor()->FindFieldByName(pb.GetDescriptor()->field(nPrimaryKeyIndex)->name());
    if (NULL == pFileDesc)
    {
        return "";
    }

    std::string sql = "delete ";

    sql += " from ";
    sql += pb.GetTypeName();
    sql += " where ";
    if (!where_clause.empty())
    {
        sql += where_clause;
    }
    else
    {
        ELOG("where_clause is empty!");
        sql = "";
    }
    return sql;
}


std::string DBTable::GetReplaceSql(MYSQL * conn, ::google::protobuf::Message &pb)
{
	std::string sql = "REPLACE INTO " + pb.GetTypeName();
	sql += " (";
	bool bNeedComma = false;
	const ::google::protobuf::Descriptor* pDesc = pb.GetDescriptor();
	const ::google::protobuf::FieldDescriptor * pFileDesc = NULL;
	for (int32_t i = 0; i < pDesc->field_count(); ++i)
	{

		if (bNeedComma == true)
		{
			sql += ", ";
		}
		else
		{
			bNeedComma = true;
		}



		sql += pDesc->field(i)->name();

	}

	sql += ") VALUES (";

	bNeedComma = false;
	for (int32_t i = 0; i < pDesc->field_count(); ++i)
	{

		if (bNeedComma == true)
		{
			sql += ", ";
		}
		else
		{
			bNeedComma = true;
		}


		pFileDesc = pDesc->FindFieldByName(pDesc->field(i)->name());
		std::string value = convertFieldValue(pb, pFileDesc);
        escapeString(value, conn);
		sql += "'";
		sql += value;
		sql += "'";
	}
	sql += ")";
	return sql;
}

std::string DBTable::getUpdateSet(MYSQL * conn, ::google::protobuf::Message &pb)
{
    std::string sql;
    const ::google::protobuf::FieldDescriptor * pFileDesc = pb.GetDescriptor()->FindFieldByName(pb.GetDescriptor()->field(nPrimaryKeyIndex)->name());
    if (NULL == pFileDesc)
    {
        return "";
    }
    bool bNeedComma = false;
    int field_count = pb.GetDescriptor()->field_count();
    for (int32_t i = 0; i < field_count; ++i)
    {
        const::std::string& field_name = pb.GetDescriptor()->field(i)->name();
        pFileDesc = pb.GetDescriptor()->FindFieldByName(field_name);
        const google::protobuf::Reflection * reflect = pb.GetReflection();
        bool hasField = reflect->HasField(pb, pFileDesc);

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

            std::string value = convertFieldValue(pb, pFileDesc);
            escapeString(value, conn);
            sql += "=";

            sql += "'";
            sql += value;
            sql += "'";
        }
    }
    return sql;
}

std::string DBTable::GetUpdateSql(MYSQL * conn, ::google::protobuf::Message &pb)
{
	const ::google::protobuf::FieldDescriptor * pFileDesc = pb.GetDescriptor()->FindFieldByName(pb.GetDescriptor()->field(nPrimaryKeyIndex)->name());
	if (NULL == pFileDesc)
	{
		return "";
	}

	std::string sql = "UPDATE " + pb.GetTypeName();
	bool bNeedComma = false;
	sql += " SET ";
    
    sql += getUpdateSet(conn, pb);

	sql += " where ";
	bNeedComma = false;
	for (auto& strPrimary : m_sPrimaryKeys)
	{
		pFileDesc = pb.GetDescriptor()->FindFieldByName(strPrimary);
		const google::protobuf::Reflection * reflect = pb.GetReflection();
		bool hasField = reflect->HasField(pb, pFileDesc);
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
			std::string value = convertFieldValue(pb, pFileDesc);
			escapeString(value, conn);
			sql += "='";
			sql += value;
			sql += "'";
		}
		
	}
	return sql;
}

std::string DBTable::GetUpdateSql(MYSQL * conn, ::google::protobuf::Message &pb, std::string where_clause)
{
    const ::google::protobuf::FieldDescriptor * pFileDesc = pb.GetDescriptor()->FindFieldByName(pb.GetDescriptor()->field(nPrimaryKeyIndex)->name());
    if (NULL == pFileDesc)
    {
        return "";
    }

    std::string sql = "UPDATE " + pb.GetTypeName();
    bool bNeedComma = false;
    sql += " SET ";
    for (int32_t i = 0; i < pb.GetDescriptor()->field_count(); ++i)
    {
        if (bNeedComma == true)
        {
            sql += ", ";
        }
        else
        {
            bNeedComma = true;
        }

        sql += " " + pb.GetDescriptor()->field(i)->name();
        pFileDesc = pb.GetDescriptor()->FindFieldByName(pb.GetDescriptor()->field(i)->name());
        std::string value = convertFieldValue(pb, pFileDesc);
        escapeString(value, conn);
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
        ELOG("where_clause is empty!");
        sql = "";
    }
    return sql;
}


std::string DBTable::GetTruncateSql(::google::protobuf::Message &pb)
{
    std::string sql = "Truncate " + pb.GetTypeName();
    return sql;
}

DBTableMng::DBTableMng()
{

}

DBTableMng::~DBTableMng()
{

}


void DBTableMng::init()
{
	
}

std::string DBTableMng::GetCreateTableSql(::google::protobuf::Message &pb)
{
	std::string tbName = pb.GetTypeName();
	db_list_type::iterator it = m_vDBList.find(tbName);
	if (it != m_vDBList.end())
	{
		return it->second.GetCreateTableSql(pb);
	}
	return "";
}

std::string DBTableMng::GetAlterTableAddFieldSql(::google::protobuf::Message &pb)
{
	std::string tbName = pb.GetTypeName();
	db_list_type::iterator it = m_vDBList.find(tbName);
	if (it != m_vDBList.end())
	{
		return it->second.GetAlterTableAddFieldSql(pb);
	}
	return "";

}

void DBTableMng::InsertFiled(const std::string & tbname, const std::string & field)
{
	db_list_type::iterator it = m_vDBList.find(tbname);
	if (it != m_vDBList.end())
	{
		return it->second.InsertFiled(field);
	}
}

std::string DBTableMng::GetInsertSql(MYSQL* _mysql, ::google::protobuf::Message &pb)
{
	std::string tbName = pb.GetTypeName();
	db_list_type::iterator it = m_vDBList.find(tbName);
	if (it != m_vDBList.end())
	{
		return it->second.GetInsertSql(_mysql, pb);
	}
	return "";
}

std::string DBTableMng::GetReplaceSql(MYSQL* _mysql, ::google::protobuf::Message &pb)
{
	std::string tbName = pb.GetTypeName();
	db_list_type::iterator it = m_vDBList.find(tbName);
	if (it != m_vDBList.end())             
	{
		return it->second.GetReplaceSql(_mysql, pb);
	}
	return "";
}

std::string DBTableMng::GetInsertOnDupUpdateSql(MYSQL* _mysql, ::google::protobuf::Message &pb)
{
    std::string tbName = pb.GetTypeName();
    db_list_type::iterator it = m_vDBList.find(tbName);
    if (it != m_vDBList.end())
    {
        return it->second.GetInsertOnDupUpdateSql(_mysql, pb);
    }
    return "";
}

std::string DBTableMng::GetUpdateSql(MYSQL* _mysql, ::google::protobuf::Message &pb)
{
	std::string tbName = pb.GetTypeName();
	db_list_type::iterator it = m_vDBList.find(tbName);
	if (it != m_vDBList.end())
	{
		return it->second.GetUpdateSql(_mysql, pb);
	}
	return "";
}

std::string DBTableMng::GetUpdateSql(MYSQL* _mysql, ::google::protobuf::Message &pb, std::string where_clause)
{
    std::string tbName = pb.GetTypeName();
    db_list_type::iterator it = m_vDBList.find(tbName);
    if (it != m_vDBList.end())
    {
        return it->second.GetUpdateSql(_mysql, pb, where_clause);
    }
    return "";
}

std::string DBTableMng::GetSelectSql(::google::protobuf::Message &pb, const std::string & key, const std::string &val)
{
    std::string tbName = pb.GetTypeName();
    db_list_type::iterator it = m_vDBList.find(tbName);
    if (it != m_vDBList.end())
    {
        return it->second.GetSelectSql(pb, key, val);
    }
    return "";
}

std::string DBTableMng::GetSelectSql(::google::protobuf::Message &pb, const std::string & where_clause)
{
	std::string tbName = pb.GetTypeName();
	db_list_type::iterator it = m_vDBList.find(tbName);
	if (it != m_vDBList.end())
	{
		return it->second.GetSelectSql(pb, where_clause);
	}
	return "";
}

std::string DBTableMng::GetSelectAllSql(::google::protobuf::Message &pb)
{
	std::string tbName = pb.GetTypeName();
	db_list_type::iterator it = m_vDBList.find(tbName);
	if (it != m_vDBList.end())
	{
		return it->second.GetSelectAllSql(pb);
	}
	return "";
}

std::string DBTableMng::GetDeleteSql(::google::protobuf::Message &pb)
{
	std::string tbName = pb.GetTypeName();
	db_list_type::iterator it = m_vDBList.find(tbName);
	if (it != m_vDBList.end())
	{
		return it->second.GetDeleteSql(pb);
	}
	return "";
}

std::string DBTableMng::GetDeleteSql(::google::protobuf::Message &pb, std::string where_clause)
{
    std::string tbName = pb.GetTypeName();
    db_list_type::iterator it = m_vDBList.find(tbName);
    if (it != m_vDBList.end())
    {
        return it->second.GetDeleteSql(pb, where_clause);
    }
    return "";
}

std::string DBTableMng::GetTruncateSql(::google::protobuf::Message &pb)
{
    std::string tbName = pb.GetTypeName();
    db_list_type::iterator it = m_vDBList.find(tbName);
    if (it != m_vDBList.end())
    {
        return it->second.GetTruncateSql(pb);
    }
    return "";
}
