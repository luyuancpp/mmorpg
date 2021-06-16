#include "database_server.h"

#include "mysql_database_table.pb.h"

namespace database
{
    void DatabaseServer::Start()
    {
        database_->AddTable(account_database::default_instance());
        database_->AddTable(player_database::default_instance());
        database_->Init();
        server_.start();
    }

    void DatabaseServer::RegisterService(google::protobuf::Service* service)
    {
        server_.registerService(service);
    }

}//namespace database


