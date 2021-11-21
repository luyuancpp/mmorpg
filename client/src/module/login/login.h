#ifndef CLIENT_SRC_MODULE_LOGIN_LOGIN_H_
#define CLIENT_SRC_MODULE_LOGIN_LOGIN_H_

#include "muduo/net/TcpConnection.h"
#include "muduo/net/TcpClient.h"

#include "src/server_common/codec/codec.h"

#include "c2gw.pb.h"

using namespace muduo;
using namespace muduo::net;

class LoginModule
{
public:
    LoginModule(ProtobufCodec& codec, TcpClient& client, TcpConnectionPtr& conn)
        : codec_(codec),
          client_(client),
          conn_(conn)
    {}

    void ReadyGo();
    void CreatePlayer();
    void EnterGame(uint64_t guid);
    void LeaveGame();
private:
    ProtobufCodec& codec_;
    TcpClient& client_;
    TcpConnectionPtr& conn_;
    
};

#endif//CLIENT_SRC_MODULE_LOGIN_LOGIN_H_
