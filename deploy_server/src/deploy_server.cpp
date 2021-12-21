#include "deploy_server.h"

#include "muduo/base/Logging.h"

#include "deploy_database_table.pb.h"

using namespace common;

deploy::DeployServer* g_deploy_server = nullptr;

double kScanOverSeconds = 3600;//扫描半个小时，特别注意，没有好的办法

namespace deploy
{
    DeployServer::DeployServer(muduo::net::EventLoop* loop, const muduo::net::InetAddress& listen_addr)
        :server_(loop, listen_addr),
         database_(std::make_shared<MysqlDatabase>())
    {
        auto& ci = DeployConfig::GetSingleton().connetion_param();
        database_->Connect(ci);
    }

    void DeployServer::Start()
    {
        database_->AddTable(region_server_db::default_instance());
        database_->AddTable(database_server_db::default_instance());
        database_->AddTable(redis_server_db::default_instance());
        database_->AddTable(login_server_db::default_instance());
        database_->AddTable(master_server_db::default_instance());
        database_->AddTable(game_server_db::default_instance());
        database_->AddTable(gateway_server_db::default_instance());
        database_->AddTable(reuse_game_server_db::default_instance());

        database_->Init();

        InitGroupDatabaseServerDb();

        InitGroupServerDb<region_server_db>(kRSBeginPort, kGroup);
        InitGroupServerDb<redis_server_db>(kRedisPort, kGroup);
        InitGroupServerDb<login_server_db>(kLSBeginPort, kGroup);
        InitGroupServerDb<master_server_db>(kMSBeginPort, kGroup);
        InitGroupServerDb<game_server_db>(kGSBeginPort, kGroup * 2);
        InitGroupServerDb<gateway_server_db>(kGateSBeginPort, kGroup);

        LoadGSDb();
        server_.subscribe<ServerConnectionES>(*this);
        server_.start();
    }

    void DeployServer::RegisterService(google::protobuf::Service* service)
    {
        server_.registerService(service);
    }

    uint32_t DeployServer::CreateGSId()
    {
        return reuse_id_.Create();
    }

    void DeployServer::SaveGSDb()
    {
        reuse_game_server_db game_server_info;
        game_server_info.set_size(reuse_id_.size());
        database_->SaveOne(game_server_info);        
    }

    void DeployServer::OnDisConnected(const muduo::net::TcpConnectionPtr& conn)
    {
        reuse_id_.OnDisConnect(conn->peerAddress().toIpPort());
        SaveGSDb();
    }

    void DeployServer::LogReuseInfo()
    {
        std::string s;
        for (auto it : reuse_id_.free_list())
        {
            s += std::to_string(it.first);
            s += " ";
        }
        LOG_INFO << "size : " << reuse_id_.size() << ", " << s;
    }

    void DeployServer::LoadGSDb()
    {
        reuse_game_server_db game_server_info;
        database_->LoadOne(game_server_info);
        reuse_id_.set_size(game_server_info.size());
        reuse_id_.OnDbLoadComplete();
        scan_over_timer_.RunAfter(kScanOverSeconds, std::bind(&ReuseGSId::ScanOver, &reuse_id_));
    }

    void DeployServer::receive(const ServerConnectionES& es)
    {
        auto& conn = es.conn_;
        if (!conn->connected())
        {
            OnDisConnected(conn);
        }
    }

    void DeployServer::InitGroupDatabaseServerDb()
    {
        auto q_result = database_->QueryOne("select * from database_server_db LIMIT 1");
        if (nullptr != q_result)
        {
            return;
        }
        auto& connetion_param = DeployConfig::GetSingleton().connetion_param();
        auto& nomoral_ip = DeployConfig::GetSingleton().deploy_param().ip();

        uint32_t region_size = 0;
        uint32_t region_id = 0;

        database_server_db sd_db;
        sd_db.set_ip(nomoral_ip);
        sd_db.set_db_host(connetion_param.db_host());
        sd_db.set_db_user("root");
        sd_db.set_db_password(connetion_param.db_password());
        sd_db.set_db_port(3306);
        sd_db.set_db_dbname("game");
        sd_db.set_region_id(region_id);

        for (uint32_t i = 0; i < kGroup; ++i)
        {
            if (region_size++ % 10 == 0)
            {
                sd_db.set_region_id(++region_id);
            }
            sd_db.set_port(i + kDSBeginPort);
            database_->SaveOne(sd_db);
        }
    }

}//namespace deploy
