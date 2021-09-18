#ifndef CLIENT_SRC_MODULE_LOGIN_LOGIN_H_
#define CLIENT_SRC_MODULE_LOGIN_LOGIN_H_

#include "muduo/net/TcpConnection.h"
#include "muduo/net/TcpClient.h"

#include "src/server_common/codec/codec.h"

#include "c2gw.pb.h"

using namespace muduo;
using namespace muduo::net;

class ClientService;

class LoginModule
{
public:
    LoginModule(ClientService* service);

    void ReadyGo();
    void CreatePlayer();
    void EnterGame(uint64_t player_id);
    void LeaveGame();
private:
    ClientService* service_{nullptr};
};

#endif//CLIENT_SRC_MODULE_LOGIN_LOGIN_H_
