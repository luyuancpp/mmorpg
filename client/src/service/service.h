#ifndef CLIENT_SRC_SERVICE_SERVICE_H_
#define CLIENT_SRC_SERVICE_SERVICE_H_

#include "muduo/net/TcpConnection.h"
#include "muduo/net/TcpClient.h"

#include "src/codec/codec.h"
#include "src/codec/dispatcher.h"
#include "src/module/login/login.h"

#include "c2gw.pb.h"

using namespace muduo;
using namespace muduo::net;

using LoginResponsePtr = std::shared_ptr<LoginResponse>;
using CreatePlayerResponePtr = std::shared_ptr<CreatePlayerRespone>;
using EnterGameResponePtr = std::shared_ptr<EnterGameRespone>;

class ClientService
{
public:
    ClientService(ProtobufDispatcher& dispatcher,
        ProtobufCodec& codec,
        TcpClient& client);

    void OnConnection(const muduo::net::TcpConnectionPtr& conn);
    void OnDisconnect();
    void ReadyGo();
    
    void OnLoginReplied(const muduo::net::TcpConnectionPtr& conn,
        const LoginResponsePtr& message,
        muduo::Timestamp);
    void OnCreatePlayerReplied(const muduo::net::TcpConnectionPtr& conn,
        const CreatePlayerResponePtr& message,
        muduo::Timestamp);
    void OnEnterGameReplied(const muduo::net::TcpConnectionPtr& conn,
        const EnterGameResponePtr& message,
        muduo::Timestamp);
    
private:
    ProtobufDispatcher& dispatcher_;
    ProtobufCodec& codec_;
    TcpConnectionPtr conn_;
    TcpClient& client_;

    LoginModule login_;

    uint64_t player_id_{ 0 };
};

#endif//CLIENT_SRC_SERVICE_SERVICE_H_
