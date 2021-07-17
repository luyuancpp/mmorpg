#include "service.h"

#include "muduo/base/Timestamp.h"

#include "src/mysql_database/mysql_database.h"
#include "src/redis_client/redis_client.h"

#include "game_database.pb.h"

namespace l2db
{
    void LoginServiceImpl::Login(::google::protobuf::RpcController* controller, 
        const l2db::LoginRequest* request,
        l2db::LoginResponse* response, 
        ::google::protobuf::Closure* done)
    {
        ::account_database& r_db = *response->mutable_account_player();
        redis_->Load(r_db, request->account());
        if (response->account_player().password().empty())
        {
            database_->LoadOne(r_db,
                std::string("account = '") + request->account() + std::string("'"));
        }
        if (r_db.password().empty())
        {
            r_db.set_account(request->account());
            r_db.set_password(request->password());
            redis_->Save(r_db, r_db.account());
        }        
        done->Run();
    }

    void LoginServiceImpl::CreatePlayer(::google::protobuf::RpcController* controller,
        const ::l2db::CreatePlayerRequest* request, 
        ::l2db::CreatePlayerResponse* response, 
        ::google::protobuf::Closure* done)
    {
        ::account_database& r_db = *response->mutable_account_player();
        redis_->Load(r_db, request->account());
        player_database new_player;
        new_player.set_register_time(muduo::Timestamp::now().microSecondsSinceEpoch());
        database_->SaveOne(new_player);
        response->set_player_id(database_->LastInsertId());
        new_player.set_player_id(response->player_id());        
        r_db.mutable_simple_players()->add_players()->set_player_id(response->player_id());
        redis_->Save(new_player, new_player.player_id());
        redis_->Save(r_db, r_db.account());
        done->Run();
    }

    void LoginServiceImpl::EnterGame(::google::protobuf::RpcController* controller, 
        const ::l2db::EnterGameRequest* request, 
        ::l2db::EnterGameResponse* response, 
        ::google::protobuf::Closure* done)
    {
        player_database new_player;
        std::string where_case = std::string("player_id = '") + 
            std::to_string(request->player_id()) + 
            std::string("'");
        database_->LoadOne(new_player, where_case);
        assert(new_player.register_time() > 0);
        redis_->Save(new_player, new_player.player_id());
        done->Run();
    }
}