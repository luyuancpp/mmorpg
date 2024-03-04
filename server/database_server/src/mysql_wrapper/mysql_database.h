#pragma once
#include "src/mysql_wrapper/mysql_client.h"
#include "src/mysql_wrapper/message_2_mysql.h"


class MysqlDatabase : public MysqlClient
{
public:

    void set_auto_increment(const ::google::protobuf::Message& message_default_instance, uint64_t auto_increment);

    void AddTable(const ::google::protobuf::Message& message_default_instance);

    void Init();

    void LoadOne(::google::protobuf::Message& message);
    void LoadOne(::google::protobuf::Message& message, const std::string& where_clause);
    template<typename T>
    void LoadAll(::google::protobuf::Message& message)
    {
        const ::google::protobuf::Descriptor* des = message.GetDescriptor();
        auto ref = message.GetReflection();
        auto fd_sub_message =
            des->FindFieldByName(des->field(0)->name());
        ::google::protobuf::MutableRepeatedFieldRef<T> rf_message = ref->GetMutableRepeatedFieldRef<T>(&message, fd_sub_message);
        T scratch_space;
        auto fill_message = [this, &rf_message, &scratch_space](const ResultRowPtr& ptr)-> bool
        {
            scratch_space.Clear();
            ParseFromString(scratch_space, *ptr);
            rf_message.Add(scratch_space);
            return true;
        };
        
        std::string sql = pb2db_.GetSelectAllSql(scratch_space);
        QueryResultRowProcessor(sql, fill_message);
    }
    template<typename T>
    void LoadAll(::google::protobuf::Message& message, const std::string& where_clause)
    {
        const ::google::protobuf::Descriptor* des = message.GetDescriptor();
        auto ref = message.GetReflection();
        auto fd_sub_message =
            des->FindFieldByName(des->field(0)->name());
        ::google::protobuf::MutableRepeatedFieldRef<T> rf_message = ref->GetMutableRepeatedFieldRef<T>(&message, fd_sub_message);
        T scratch_space;
        auto fill_message = [this, &rf_message, &scratch_space](const ResultRowPtr& ptr)-> bool
        {
            scratch_space.Clear();
            ParseFromString(scratch_space, *ptr);
            rf_message.Add(scratch_space);
            return true;
        };

        std::string sql = pb2db_.GetSelectAllSql(scratch_space, where_clause);
        QueryResultRowProcessor(sql, fill_message);
    }

    void SaveOne(const ::google::protobuf::Message& message);
    template<typename T>
    void SaveAll(const ::google::protobuf::Message& message)
    {
        const ::google::protobuf::Descriptor* des = message.GetDescriptor();
        auto ref = message.GetReflection();
        auto fd_sub_message =
            des->FindFieldByName(des->field(0)->name());
        auto rf_message = ref->GetRepeatedFieldRef<T>(message, fd_sub_message);
        for (auto it = rf_message.begin(); it != rf_message.end(); ++it)
        {
            Execute(pb2db_.GetInsertOnDupKeyForPrimaryKey(*it));
        }
    }

    void Delete(const ::google::protobuf::Message& message);
    void Delete(const ::google::protobuf::Message& message, const std::string& where_clause);    

private:
    Pb2DbTables pb2db_;
};

using MysqlClientPtr = std::shared_ptr<MysqlDatabase>;