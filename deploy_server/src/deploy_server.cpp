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

        InitGroupLoginServerDb<region_server_db>(kRegionServerBeginPort, kGroup);
        InitGroupLoginServerDb<redis_server_db>(kRedisPort, kGroup);
        InitGroupLoginServerDb<login_server_db>(kLoginServerBeginPort, kGroup);
        InitGroupLoginServerDb<master_server_db>(kMasterServerBeginPort, kGroup);
        InitGroupLoginServerDb<game_server_db>(kGameServerBeginPort, kGroup * 2);
        InitGroupLoginServerDb<gateway_server_db>(kGatewayServerBeginPort, kGroup);

        LoadGameServerDb();
        server_.subscribe<ServerConnectionES>(*this);
        server_.start();
    }

    void DeployServer::RegisterService(google::protobuf::Service* service)
    {
        server_.registerService(service);
    }

    uint32_t DeployServer::CreateGameServerId()
    {
        return reuse_id_.Create();
    }

    void DeployServer::SaveGameServerDb()
    {
        reuse_game_server_db game_server_info;
        game_server_info.set_current_size(reuse_id_.size());
        *game_server_info.mutable_free_list()->mutable_free_list() = reuse_id_.free_list();
        database_->SaveOne(game_server_info);        
    }

    void DeployServer::OnDisConnected(const muduo::net::TcpConnectionPtr& conn)
    {
        reuse_id_.OnDisConnect(conn->peerAddress().toIpPort());
        SaveGameServerDb();
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

    void DeployServer::LoadGameServerDb()
    {
        reuse_game_server_db game_server_info;
        database_->LoadOne(game_server_info);
        reuse_id_.set_size(game_server_info.current_size());
        reuse_id_.OnDbLoadComplete();
        scan_over_timer_.RunAfter(kScanOverSeconds, std::bind(&ReuseGameServerId::ScanOver, &reuse_id_));
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
            sd_db.set_port(i + kDatabeseServerBeginPort);
            database_->SaveOne(sd_db);
        }
    }

}//namespace deploy
