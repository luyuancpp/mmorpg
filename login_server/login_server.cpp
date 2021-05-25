#include "gw2l.pb.h"

#include "muduo/base/Logging.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/protorpc/RpcServer.h"
#ifdef __linux__
#include <unistd.h>
#endif//__linux__

using namespace muduo;
using namespace muduo::net;

namespace gw2l
{
class LoginServiceImpl : public LoginService
{
public:
    virtual void Login(::google::protobuf::RpcController* controller,
        const gw2l::LoginRequest* request,
        gw2l::LoginResponse* response,
        ::google::protobuf::Closure* done)override
    {
        LOG_INFO << "login : " << request->DebugString();
        done->Run();
    }
};

}  // namespace echo

int main(int argc, char* argv[])
{
    int nThreads = argc > 1 ? atoi(argv[1]) : 1;
    EventLoop loop;
    InetAddress listenAddr("127.0.0.1", 2001);
    gw2l::LoginServiceImpl impl;
    RpcServer server(&loop, listenAddr);
    server.setThreadNum(nThreads);
    server.registerService(&impl);
    server.start();
    loop.loop();
}

