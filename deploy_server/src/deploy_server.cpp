#include "deploy_server.h"

#include "muduo/base/Logging.h"

#include "src/deploy_variable.h"

#include "mysql_database_table.pb.h"

deploy::DeployServer* g_deploy_server = nullptr;

double kScanOverSeconds = 60;//扫描半个小时，特别注意，没有好的办法

namespace deploy
{
    DeployServer::DeployServer(muduo::net::EventLoop* loop, const muduo::net::InetAddress& listen_addr)
        :server_(loop, listen_addr),
         db_(std::make_shared<MysqlClientPtr::element_type>())
    {
        auto& ci = DeployConfig::GetSingleton().database_param();
        db_->Connect(ci);
    }

    void DeployServer::Start()
    {
		impl_.set_player_mysql_client(player_mysql_client());
		RegisterService(&impl_);

        g_deploy_server = this;
        db_->AddTable(region_server_db::default_instance());
        db_->AddTable(database_server_db::default_instance());
        db_->AddTable(redis_server_db::default_instance());
        db_->AddTable(login_server_db::default_instance());
        db_->AddTable(master_server_db::default_instance());
        db_->AddTable(game_server_db::default_instance());
        db_->AddTable(gateway_server_db::default_instance());
        db_->AddTable(reuse_game_server_db::default_instance());

        db_->Init();

        InitGroupDb();

        InitRegionServer<region_server_db>(kRSBeginPort, kGroup);
        InitGroupDb<redis_server_db>(kRedisPort, kGroup);
        InitGroupDb<login_server_db>(kLSBeginPort, kGroup);
        InitRegionDb<master_server_db>(kMSBeginPort, kGroup);
        InitRegionDb<game_server_db>(kGSBeginPort, kGroup * 2);
        InitRegionDb<gateway_server_db>(kGateSBeginPort, kGroup);

        LoadGSDb();
        server_.subscribe<OnBeConnectedEvent>(*this);
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
        db_->SaveOne(game_server_info);        
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
        db_->LoadOne(game_server_info);
        reuse_id_.set_size(game_server_info.size());
        reuse_id_.OnDbLoadComplete();
        scan_over_timer_.RunAfter(kScanOverSeconds, std::bind(&ReuseGsId::ScanOver, &reuse_id_));
    }

    void DeployServer::receive(const OnBeConnectedEvent& es)
    {
        auto& conn = es.conn_;
        if (!conn->connected())
        {
            OnDisConnected(conn);
        }
    }

    void DeployServer::InitGroupDb()
    {
        auto q_result = db_->QueryOne("select * from database_server_db LIMIT 1");
        if (nullptr != q_result)
        {
            return;
        }
        auto& connection_info = DeployConfig::GetSingleton().database_param();
        auto& node_ip = DeployConfig::GetSingleton().deploy_info().ip();

        uint32_t region_size = 0;
        uint32_t region_id = 0;

        database_server_db db_row;
        db_row.set_ip(node_ip);
        db_row.set_db_host(connection_info.db_host());
        db_row.set_db_user("root");
        db_row.set_db_password(connection_info.db_password());
        db_row.set_db_port(3306);
        db_row.set_db_dbname("game");
        db_row.set_region_id(region_id);

        for (uint32_t i = 0; i < kGroup; ++i)
        {
            if (region_size++ % 10 == 0)
            {
                db_row.set_region_id(++region_id);
            }
            db_row.set_port(i + kDSBeginPort);
            db_->SaveOne(db_row);
        }
    }

}//namespace deploy
