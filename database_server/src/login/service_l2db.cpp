#include "service_l2db.h"

#include "muduo/base/Timestamp.h"

#include "src/mysql_database/mysql_database.h"
#include "src/server_common/closure_auto_done.h"
#include "src/redis_client/redis_client.h"

#include "comp.pb.h"

using namespace common;

namespace l2db
{
    void LoginServiceImpl::Login(::google::protobuf::RpcController* controller, 
        const l2db::LoginRequest* request,
        l2db::LoginResponse* response, 
        ::google::protobuf::Closure* done)
    {
        AutoRecycleClosure cp(done);
        ::account_database& db = *response->mutable_account_player();
        auto& account = request->account();
        redis_->Load(db, account);
        if (response->account_player().password().empty())
        {
            database_->LoadOne(db,
                std::string("account = '") + account + std::string("'"));
        }
        if (db.password().empty())
        {
            db.set_account(account);
            redis_->Save(db, account);
        }        
    }

    void LoginServiceImpl::CreatePlayer(::google::protobuf::RpcController* controller,
        const ::l2db::CreatePlayerRequest* request, 
        ::l2db::CreatePlayerResponse* response, 
        ::google::protobuf::Closure* done)
    {
        AutoRecycleClosure cp(done);
        ::account_database& r_db = *response->mutable_account_player();
        redis_->Load(r_db, request->account());
        player_database new_player;
        new_player.set_register_time(muduo::Timestamp::now().microSecondsSinceEpoch());
        database_->SaveOne(new_player);
        response->set_guid(database_->LastInsertId());
        new_player.set_guid(response->guid());        
        r_db.mutable_simple_players()->add_players()->set_guid(response->guid());
        redis_->Save(new_player, new_player.guid());
        redis_->Save(r_db, r_db.account());
    }

    void LoginServiceImpl::EnterGame(::google::protobuf::RpcController* controller, 
        const ::l2db::EnterGameRequest* request, 
        ::l2db::EnterGameResponse* response, 
        ::google::protobuf::Closure* done)
    {
        AutoRecycleClosure cp(done);
        player_database new_player;
        std::string where_case = std::string("guid = '") + 
            std::to_string(request->guid()) + 
            std::string("'");
        database_->LoadOne(new_player, where_case);
        assert(new_player.guid() > 0);
        redis_->Save(new_player, new_player.guid());
    }
}