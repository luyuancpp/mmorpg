#include "service.h"

#include "muduo/base/Logging.h"

#include "c2gw.pb.h"

void ClientService::OnConnection(const muduo::net::TcpConnectionPtr& conn)
{
    conn_ = conn;
}



void ClientService::ReadyGo()
{
    LoginRequest query;
    query.set_account("luhailong11");
    query.set_password("lhl.2021");
    codec_.send(conn_, query);
}

void ClientService::OnLoginReplied(const muduo::net::TcpConnectionPtr& conn, const LoginResponsePtr& message, muduo::Timestamp)
{
    LOG_INFO << "login: " << message->DebugString();
    all_finished_->countDown();
}

