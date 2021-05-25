#include "database_rpcclient.h"

namespace login
{
void DatabaseRpcClient::Login(const l2db::LoginRequest& request)
{
    database_client_->SendRequest<l2db::LoginRequest, l2db::LoginResponse>
        (request, this, &DatabaseRpcClient::LoginReplied, &l2db::LoginService_Stub::Login);
}

void DatabaseRpcClient::LoginReplied(l2db::LoginResponse* response)
{

}

}//namespace login


