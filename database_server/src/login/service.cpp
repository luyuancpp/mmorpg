#include "service.h"

#include "src/mysql_database/mysql_database.h"
#include "src/redis_client/redis_client.h"

namespace l2db
{
    void LoginServiceImpl::Login(::google::protobuf::RpcController* controller, 
        const l2db::LoginRequest* request,
        l2db::LoginResponse* response, 
        ::google::protobuf::Closure* done)
    {
        ::account_database& response_account_database = *response->mutable_player_account();
        redis_->Load(response_account_database, request->account());
        if (response->player_account().password().empty())
        {
            database_->LoadOne(response_account_database,
                std::string("account = ") + request->account());
        }
        if (response_account_database.password().empty())
        {
            response_account_database.set_account(request->account());
            response_account_database.set_password(request->password());
            redis_->Save(response_account_database, response_account_database.account());
        }        
        done->Run();
    }

}