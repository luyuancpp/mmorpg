#include "database_server.h"

#include "src/deploy/rpcclient/deploy_rpcclient.h"
#include "src/server_type_id/server_type_id.h"

#include "mysql_database_table.pb.h"

namespace database
{

    DatabaseServer::DatabaseServer(muduo::net::EventLoop* loop)
        : loop_(loop),
          database_(std::make_shared<common::MysqlDatabase>()),
          redis_(std::make_shared<common::RedisClient>())
    {
        DeployRpcClient::GetSingleton()->emp()->subscribe<common::ConnectionEvent>(*this);
        
    }

    DatabaseServer::~DatabaseServer()
    {
        DeployRpcClient::GetSingleton()->emp()->unsubscribe<common::ConnectionEvent>(*this);
    }

    void DatabaseServer::Start()
    {
        database_->AddTable(account_database::default_instance());
        database_->AddTable(player_database::default_instance());
        database_->Init();
        server_->start();
    }

    void DatabaseServer::RegisterService(google::protobuf::Service* service)
    {
        server_->registerService(service);
    }

    void DatabaseServer::receive(const common::ConnectionEvent& es)
    {
        if (!es.conn_->connected())
        {
            return;
        }
        ServerInfoRpcRC cp(std::make_shared<ServerInfoRpcClosure>());
        cp->s_reqst_.set_group(0);
        cp->s_reqst_.set_server_type_id(common::SERVER_DATABASE);
        database::ServerInfoRpcStub::GetSingleton().CallMethod(
            &DatabaseServer::DbServerInfoReplied,
            cp,
            this,
            &deploy::DeployService_Stub::ServerInfo);
    }

    void DatabaseServer::DbServerInfoReplied(ServerInfoRpcRC cp)
    {
        InetAddress listenAddr(cp->s_resp_->info().ip(), cp->s_resp_->info().port());
        redis_->Connect(cp->s_resp_->info().ip(), 6379, 1, 1);
        ConnetionParam query_database_param;
        query_database_param.set_host_name(cp->s_resp_->info().db_host());
        query_database_param.set_user_name(cp->s_resp_->info().db_user());
        query_database_param.set_pass_word(cp->s_resp_->info().db_password());
        query_database_param.set_database_name(cp->s_resp_->info().db_dbname());
        query_database_param.set_port(cp->s_resp_->info().db_port());
        database_->Connect(query_database_param);
        server_ = std::make_shared<muduo::net::RpcServer>(loop_, listenAddr);
    }


}//namespace database


