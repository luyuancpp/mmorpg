#pragma once
#include "Util.h"
#include <cctype>
#include "mysql.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

struct MysqlField
{
    char* field_chars;
    int field_len;
};


class DBTable
{
public:
	typedef std::unordered_set<std::string> field_list_type;
	typedef std::vector<std::string> string_vector_type;
	DBTable();
	~DBTable();

	bool Init();

	
	std::string GetCreateTableSql(::google::protobuf::Message &pb);

	std::string GetAlterTableAddFieldSql(::google::protobuf::Message &pb);

	void InsertFiled(const std::string & field)
	{
        m_vFieldList.insert(field);
	}


	std::string GetInsertSql(MYSQL * conn, ::google::protobuf::Message &pb);

	std::string GetSelectSql(::google::protobuf::Message &pb, const std::string & key, const std::string &val);

    std::string GetSelectSql(::google::protobuf::Message &pb, const std::string & where_clause);

	std::string GetSelectAllSql(::google::protobuf::Message &pb);

    std::string GetInsertOnDupUpdateSql(MYSQL *conn, ::google::protobuf::Message &pb);

	std::string GetDeleteSql(::google::protobuf::Message &pb);
    std::string GetDeleteSql(::google::protobuf::Message &pb, const std::string & where_clause);

	std::string GetReplaceSql(MYSQL * conn, ::google::protobuf::Message &pb);

	std::string GetUpdateSql(MYSQL *conn, ::google::protobuf::Message &pb);
    std::string GetUpdateSql(MYSQL* _mysql, ::google::protobuf::Message &pb, std::string where_clause);

    std::string GetTruncateSql(::google::protobuf::Message &pb);

	static void fillMessageField(::google::protobuf::Message &pb, MysqlField * fields);
    void escapeString(std::string &str, MYSQL* conn);
private:
    std::string getUpdateSet(MYSQL * conn, ::google::protobuf::Message &pb);
private:
	

	field_list_type m_vFieldList;
	string_vector_type m_sPrimaryKeys;
    string_vector_type m_sIndexes;
    string_vector_type m_vUniqueKeys;
    std::string m_sForeignKeys;
    std::string m_sForeignReferences;
    std::string m_sAutoIncreaseKey;
};

class DBTableMng
{
public:
	
	typedef std::unordered_map<std::string, DBTable> db_list_type;
	typedef std::shared_ptr<::google::protobuf::Message> pb_ptr;
	typedef std::unordered_set<pb_ptr> pb_tb_list;
	typedef std::unordered_map<std::string, std::string> pbname_dbname_type;

	DBTableMng();
	~DBTableMng();

	virtual void init();

	std::string GetCreateTableSql(::google::protobuf::Message &pb);

	std::string GetAlterTableAddFieldSql(::google::protobuf::Message &pb);

	std::string GetInsertSql(MYSQL* _mysql, ::google::protobuf::Message &pb);

	std::string GetReplaceSql(MYSQL* _mysql,::google::protobuf::Message &pb);
    std::string GetInsertOnDupUpdateSql(MYSQL* _mysql, ::google::protobuf::Message &pb);

	std::string GetUpdateSql(MYSQL* mysql, ::google::protobuf::Message &pb);
    std::string GetUpdateSql(MYSQL* _mysql, ::google::protobuf::Message &pb, std::string where_clause);

	std::string GetSelectSql(::google::protobuf::Message &pb, const std::string & key, const std::string &val);
    std::string GetSelectSql(::google::protobuf::Message &pb, const std::string & where_clause);

	std::string GetSelectAllSql(::google::protobuf::Message &pb);

	std::string GetDeleteSql(::google::protobuf::Message &pb);
    std::string GetDeleteSql(::google::protobuf::Message &pb, std::string where_clause);

    std::string GetTruncateSql(::google::protobuf::Message &pb);

	const pb_tb_list & GetPbTableList()
	{
		return m_vPbList;
	}

	void InsertFiled(const std::string & tbname, const std::string & field);

//private:
	db_list_type m_vDBList;
	pb_tb_list m_vPbList;
	static std::string m_inc_base;
};
