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
        virtual void Echo(::google::protobuf::RpcController* controller,
            const LoginRequest* request,
            LoginResponse* response,
            ::google::protobuf::Closure* done)
        {
            //LOG_INFO << "EchoServiceImpl::Solve";
            done->Run();
        }
    };

}  // namespace echo

int main(int argc, char* argv[])
{
    int nThreads = argc > 1 ? atoi(argv[1]) : 1;
    LOG_INFO << "pid = " << getpid() << " threads = " << nThreads;
    EventLoop loop;
    int port = argc > 2 ? atoi(argv[2]) : 8888;
    InetAddress listenAddr(static_cast<uint16_t>(port));
    gw2l::LoginServiceImpl impl;
    RpcServer server(&loop, listenAddr);
    server.setThreadNum(nThreads);
    server.registerService(&impl);
    server.start();
    loop.loop();
}

