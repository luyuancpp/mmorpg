#include "l2db.pb.h"

#include "muduo/base/Logging.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/protorpc/RpcServer.h"
#ifdef __linux__
#include <unistd.h>
#endif//__linux__

using namespace muduo;
using namespace muduo::net;

namespace l2db
{
    class LoginServiceImpl : public LoginService
    {
    public:
        virtual void Login(::google::protobuf::RpcController* controller,
            const l2db::LoginRequest* request,
            l2db::LoginResponse* response,
            ::google::protobuf::Closure* done)override
        {
            LOG_INFO << "login : " << request->DebugString();
            done->Run();
        }
    };

}  // namespace l2db

int main(int argc, char* argv[])
{
    int nThreads = argc > 1 ? atoi(argv[1]) : 1;
    EventLoop loop;
    InetAddress listenAddr("127.0.0.1", 2002);
    l2db::LoginServiceImpl impl;
    RpcServer server(&loop, listenAddr);
    server.setThreadNum(nThreads);
    server.registerService(&impl);
    server.start();
    loop.loop();
}

