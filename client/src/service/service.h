#ifndef CLIENT_SRC_SERVICE_SERVICE_H_
#define CLIENT_SRC_SERVICE_SERVICE_H_

#include "muduo/net/TcpConnection.h"
#include "muduo/net/TcpClient.h"

#include "src/codec/codec.h"

class LoginResponse;

using LoginResponsePtr =  std::shared_ptr<LoginResponse> ;

using namespace muduo;
using namespace muduo::net;

class ClientService
{
public:
    ClientService(ProtobufCodec& codec,  TcpClient& client)
        : codec_(codec),
          client_(client)
    {}
    void OnConnection(const muduo::net::TcpConnectionPtr& conn);
    void OnDisconnect();
    void ReadyGo();
    
    void OnLoginReplied(const muduo::net::TcpConnectionPtr& conn,
        const LoginResponsePtr& message,
        muduo::Timestamp);
    
private:
    ProtobufCodec& codec_;
    TcpConnectionPtr conn_;
    TcpClient& client_;
};

#endif//CLIENT_SRC_SERVICE_SERVICE_H_
