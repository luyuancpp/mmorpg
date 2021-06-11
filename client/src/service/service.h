#ifndef CLIENT_SRC_SERVICE_SERVICE_H_
#define CLIENT_SRC_SERVICE_SERVICE_H_

#include "muduo/base/CountDownLatch.h"
#include "muduo/net/TcpConnection.h"

#include "src/codec/codec.h"

class LoginResponse;

using LoginResponsePtr =  std::shared_ptr<LoginResponse> ;

using namespace muduo;
using namespace muduo::net;

class ClientService
{
public:
    ClientService(ProtobufCodec& codec, CountDownLatch* all_finished) 
        : codec_(codec),
          all_finished_(all_finished){}
    void OnConnection(const muduo::net::TcpConnectionPtr& conn);
    void OnDisconnect();
    void ReadyGo();
    
    void OnLoginReplied(const muduo::net::TcpConnectionPtr& conn,
        const LoginResponsePtr& message,
        muduo::Timestamp);
    
private:
    ProtobufCodec& codec_;
    TcpConnectionPtr conn_;
    CountDownLatch* all_finished_{ nullptr };
};

#endif//CLIENT_SRC_SERVICE_SERVICE_H_
