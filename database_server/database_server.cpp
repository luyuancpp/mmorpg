#include "database_server.h"

#include "mysql_database_table.pb.h"
#include "src/login/service.h"

#include "l2db.pb.h"

namespace database
{

    void DatabaseServer::Start()
    {
        l2db::LoginServiceImpl impl;
        server_.registerService(&impl);
        database_->AddTable(account_database::default_instance());
        database_->Init();
        server_.start();
    }

}//namespace database


