﻿#include "deploy_server.h"

#include "muduo/base/Logging.h"
#include "src/game_config/deploy_json.h"
#include "src/server_common/deploy_variable.h"

#include "deploy_database_table.pb.h"

deploy::DeployServer* g_deploy_server = nullptr;

double kScanOverTime = 5;

namespace deploy
{
    DeployServer::DeployServer(muduo::net::EventLoop* loop,
        const muduo::net::InetAddress& listen_addr)
        :server_(loop, listen_addr),
        database_(std::make_shared<common::MysqlDatabase>())
    {
        auto& ci = common::DeployConfig::GetSingleton().connetion_param();
        auto& connetion_param = common::DeployConfig::GetSingleton().connetion_param();
        nomoral_database_ip_ = connetion_param.db_host();
        nomoral_password_ = connetion_param.db_password();
        nomoral_ip_ = common::DeployConfig::GetSingleton().deploy_param().ip();
        database_->Connect(ci);
    }

    void DeployServer::Start()
    {
        database_->AddTable(group_server_db::default_instance());
        database_->AddTable(game_server_db::default_instance());
        database_->Init();
        InitGroupServerDb();
        LoadGameServerDb();
        server_.subscribe<common::ServerConnectionES>(*this);
        server_.start();
    }

    void DeployServer::RegisterService(google::protobuf::Service* service)
    {
        server_.registerService(service);
    }

    uint32_t DeployServer::CreateGameServerId()
    {
        if (reuse_id_.IsScanEmpty())
        {
            return reuse_id_.Create();
        }
        return reuse_id_.CreateNoReuse();
    }

    void DeployServer::SaveGameServerDb()
    {
        game_server_db game_server_info;
        game_server_info.set_current_size(reuse_id_.size());
        *game_server_info.mutable_free_list()->mutable_free_list() = reuse_id_.free_list();
        database_->SaveOne(game_server_info);        
    }

    void DeployServer::OnDisConnected(const muduo::net::TcpConnectionPtr& conn)
    {
        reuse_id_.OnConnect(conn->peerAddress().toIpPort());
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
        game_server_db game_server_info;
        database_->LoadOne(game_server_info);
        reuse_id_.set_size(game_server_info.current_size());
        reuse_id_.OnDbLoadComplete();
        reuse_id_.set_free_list(game_server_info.free_list().free_list());

        scan_over_timer_.RunAfter(kScanOverTime, std::bind(&ReuseGameServerId::ScanOver, &reuse_id_));
    }

    void DeployServer::receive(const common::ServerConnectionES& es)
    {
        auto& conn = es.conn_;
        if (!conn->connected())
        {
            OnDisConnected(conn);
        }
    }

    void DeployServer::InitGroupServerDb()
    {
        auto q_result = database_->QueryOne("select * from group_server_db LIMIT 1");
        if (nullptr != q_result)
        {
            return;
        }
        group_server_db sd_db;
        sd_db.set_ip(nomoral_ip_);
        sd_db.set_db_host(nomoral_database_ip_);
        sd_db.set_db_user("root");
        sd_db.set_db_password(nomoral_password_);
        sd_db.set_db_port(3306);
        sd_db.set_db_dbname("game");

        group_server_db sd_nodb;
        sd_nodb.set_ip(nomoral_ip_);

        group_server_db sd_redis;
        sd_redis.set_ip(redis_ip_);
        sd_redis.set_port(kRedisPort);

        for (uint32_t i = 0; i < kTotalSize; ++i)
        {
            if (i % common::SERVER_ID_GROUP_SIZE == 0)
            {
                database_->SaveOne(sd_redis);
                continue;
            }
            sd_db.set_port(kBeginPort + i);
            sd_nodb.set_port(sd_db.port());
            if (i % common::SERVER_ID_GROUP_SIZE == common::SERVER_DATABASE)
            {
                database_->SaveOne(sd_db);
            }
            else
            {
                database_->SaveOne(sd_nodb);
            }
        }
    }

}//namespace deploy
