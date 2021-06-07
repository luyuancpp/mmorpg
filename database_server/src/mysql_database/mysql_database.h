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

    void LoadOne(::google::protobuf::Message& message);

    template<typename T>
    void LoadAll(::google::protobuf::Message& message)
    {
        const ::google::protobuf::Descriptor* des = message.GetDescriptor();
        if (des->field_count() != 1)
        {
            return;
        }
        auto ref = message.GetReflection();
        auto fd_sub_message =
            des->FindFieldByName(des->field(0)->name());
        auto rf_message = ref->GetMutableRepeatedFieldRef<T>(&message, fd_sub_message);
        auto fill_message = [this, &rf_message](const ResultRowPtr& ptr)-> bool
        {
            common::FillMessageField(*rf_message.NewMessage(), *ptr);
            return true;
        };
        T sqlmessage;
        std::string sql = GetSelectAllSql(sqlmessage);
        //Query(sql, fill_message);
    }

    void SaveOne(const ::google::protobuf::Message& message);

    template<typename T>
    void SaveAll(const ::google::protobuf::Message& message)
    {
        const ::google::protobuf::Descriptor* des = message.GetDescriptor();
        if (des->field_count() != 1)
        {
            return ;
        }
        auto ref = message.GetReflection();
        auto fd_sub_message =
            des->FindFieldByName(des->field(0)->name());
        auto rf_message = ref->GetRepeatedFieldRef<T>(message, fd_sub_message);
        for (auto it = rf_message.begin(); it != rf_message.end(); ++it)
        {
            Execute(GetInsertOnDupKeyForPririmarykey(*it));
        }
    }

private:

};

}//namespace database

#endif // !DATA_BASE_SRC_MYSQL_DATA_BASE_MYSQL_DATABASE_H_
