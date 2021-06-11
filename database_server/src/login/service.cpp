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
        ::account_database& r_db = *response->mutable_account_player();
        redis_->Load(r_db, request->account());
        if (response->account_player().password().empty())
        {
            database_->LoadOne(r_db,
                std::string("account = ") + request->account());
        }
        if (r_db.password().empty())
        {
            r_db.set_account(request->account());
            r_db.set_password(request->password());
            redis_->Save(r_db, r_db.account());
        }        
        done->Run();
    }

}