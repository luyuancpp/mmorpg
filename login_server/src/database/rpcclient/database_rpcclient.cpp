#include "database_rpcclient.h"

namespace login
{
void DbRpcClient::Login(const l2db::LoginRequest& request)
{
    database_client_->SendRequest<l2db::LoginRequest, l2db::LoginResponse>
        (request, this, &DbRpcClient::LoginReplied, &l2db::LoginService_Stub::Login);
}

void DbRpcClient::LoginReplied(l2db::LoginResponse* response)
{
    std::unique_ptr<google::protobuf::Message> d(response);
}

}//namespace login


