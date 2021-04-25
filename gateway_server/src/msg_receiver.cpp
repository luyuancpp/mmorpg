#include "msg_receiver.h"

#include "muduo/base/Logging.h"
#include "muduo/net/TcpConnection.h"

void gateway::MsgReceiver::onAnswer(const muduo::net::TcpConnectionPtr& conn, 
                                    const LoginRequestPtr& message,
                                    muduo::Timestamp)
{
    LOG_INFO << "onAnswer: " << message->DebugString();
    LoginRespone respone;
    codec_.send(conn, respone);
    conn->shutdown();
}

