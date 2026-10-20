// grpc_runtime_not_initialized_test — 「官方 gRPC 运行时未初始化」回归用例(gRPC 迁到 muduo 驱动第 5 批;
// docs/design/thread-count-monitoring_zh.md「gRPC 运行时守卫」,docs/design/grpc-muduo-transport.md §14、§15)。
//
// 负责人的目标是线程数可控:grpc 库照常链接,但节点内不得启动任何官方 gRPC 线程。第 3、4 批之后生成的客户端
// 与服务端都走 muduo_grpc(跑在调用方自己的 EventLoop 上,不建线程),官方运行时(grpc_init)应始终未初始化。
// 这里把两条真实路径各走一遍,之后用 node::observability::IsGrpcRuntimeInitialized()(grpc_is_initialized())
// 断言官方运行时仍未初始化:
//   * 生成客户端的本地拒绝:节点实体上没有通道,Send 经 QueueGrpcLocalCompletion 以 UNAVAILABLE 完成;
//   * 一次真实的 muduo_grpc 调用:生成客户端 → muduo_grpc::Channel → 同进程 muduo_grpc::Server → core 的
//     GrpcUnaryCall 胶水(与 scene / battle 生成骨架同一条路径)→ OK 应答;拆除(~Channel、~Server)之后再断言一次。
// 若将来有人在这些路径里引入官方 grpc 对象(或 include 的生成代码隐式调了 grpc_init),这里会失败。
// 运行期还有同一条守卫:thread_observability.h 在注册与每次采样时核对,第一次为真即打 ERROR。
//
// 约束(重要):本测试进程(rpc_controller_test,与 rpc_controller_test.cpp 同一工程、同一个 main)里**不得有任何
// 用例构造官方 grpc 对象**(grpc::Channel / Server / CompletionQueue / 凭据 / ResourceQuota / Alarm、要先有官方通道的
// *.grpc.pb.h Stub,或直接 grpc_init())。grpc_is_initialized() 是进程级状态,grpc_shutdown 还可能在后台线程
// 异步收尾,任何一个这样的用例都会让本文件的结论取决于用例执行顺序。出于同一原因,这里刻意不做「先 grpc_init 再
// 断言为真」的正向对照。
// 前置条件在 SetUp 里断言:进入用例时官方运行时就已初始化,说明同进程有代码违反了上面这条约束。
//
// 全部在测试线程自己的 EventLoop 上:Server、通道、连接都不起线程。等待一律以 5s 作活性上限,判据只看事件与计数。
// 销毁顺序同 grpc_client_loopback_test.cpp:通道先于 Server(先转 loop 让连接断开、排队的完成回调跑完),Server 在
// loop 外析构(server.h 的前置条件),之后再转 loop 让它排进的 forceClose / connectDestroyed 跑完;EventLoop 最先构造、
// 最后析构。

#include <gtest/gtest.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

// muduo 头放在标准库 / 系统头之后(它的 Windows 适配头 #define 了 off_t / pid_t 等名字)。
#include "core/network/grpc_client_runtime.h"
#include "core/network/grpc_unary_call.h"
#include "core/node/system/node/grpc_runtime_probe.h"
#include "grpc_client/data_service/data_service_grpc_client.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/TimerId.h"
#include "muduo_grpc/channel.h"
#include "muduo_grpc/method_table.h"
#include "muduo_grpc/server.h"
#include "muduo_grpc/server_call.h"

using namespace std::chrono_literals;

namespace
{

using data_service::LoadPlayerDataRequest;
using data_service::LoadPlayerDataResponse;
using LoadPlayerDataCall = GrpcUnaryCall<LoadPlayerDataRequest, LoadPlayerDataResponse>;
using node::observability::IsGrpcRuntimeInitialized;

constexpr auto kLiveness = 5s;   // 只作活性上限,不作判据

// 向 OS 借一个空闲回环端口(Server::Create 不接受 0 端口)。失败返回 0。
// 与 grpc_client_loopback_test.cpp 里的同名函数相同:两个用例文件各自独立,不为一个测试小工具另立共享头。
uint16_t FindFreeLoopbackPort()
{
#ifdef _WIN32
    const SOCKET s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET)
    {
        return 0;
    }
    int len = sizeof(sockaddr_in);
#else
    const int s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s < 0)
    {
        return 0;
    }
    socklen_t len = sizeof(sockaddr_in);
#endif
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    uint16_t port = 0;
    if (::bind(s, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) == 0 &&
        ::getsockname(s, reinterpret_cast<sockaddr *>(&addr), &len) == 0)
    {
        port = ntohs(addr.sin_port);
    }
#ifdef _WIN32
    ::closesocket(s);
#else
    ::close(s);
#endif
    return port;
}

// 测试服务:同步方法体(Proceed → 业务 → FinishOk),走 core 的 GrpcUnaryCall。只在 loop 线程上使用。
// 必须活过注册它的 Server(方法表闭包捕获 this),所以在夹具里先于 server_ 声明。
class EchoDataService final : public muduo_grpc::Service
{
public:
    void RegisterMethods(muduo_grpc::MethodTable &table) override
    {
        table.AddUnary(std::string(data_service::kDataServiceLoadPlayerDataGrpcPath),
                       [this](muduo_grpc::ServerCall serverCall, std::string_view requestBytes)
                       { Dispatch(std::move(serverCall), requestBytes); });
    }

    int businessRuns{0};

private:
    void Dispatch(muduo_grpc::ServerCall serverCall, std::string_view requestBytes)
    {
        const auto call = LoadPlayerDataCall::Create(
            std::move(serverCall),
            [this](const LoadPlayerDataRequest *request, LoadPlayerDataResponse *response)
            {
                ++businessRuns;
                response->set_version(request->player_id() + 1);
            });
        if (!call->ParseRequest(requestBytes))
        {
            return;
        }
        call->Proceed();
    }
};

// 一次客户端调用的观察结果。回调只在 loop 线程执行。
struct Observed
{
    int calls{0};
    uint32_t code{kGrpcStatusOk};
    std::string message;
    bool hadReply{false};
    uint64_t version{0};
};

class GrpcRuntimeNotInitializedTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // 前置条件:进入用例时官方运行时未初始化。探针自身的首次调用不会把它变成 true(见 grpc_runtime_probe.h)。
        ASSERT_FALSE(IsGrpcRuntimeInitialized())
            << "official gRPC runtime was already initialized before this test: some code in this test process "
               "constructed official grpc objects or called grpc_init()";
    }

    void TearDown() override
    {
        if (server_ != nullptr || channel_ != nullptr)
        {
            // 通道先走:~Channel 把在途调用以 UNAVAILABLE 排进队列,连接断开后服务端摘表。
            registry_.clear();
            channel_.reset();
            RunLoopFor(300ms);
            // loop 外析构 Server(server.h 的前置条件),再转 loop 让它排进的 forceClose / connectDestroyed 跑完。
            server_.reset();
            RunLoopFor(200ms);
        }
        // 拆除路径同样不得初始化官方运行时。
        EXPECT_FALSE(IsGrpcRuntimeInitialized()) << "teardown initialized the official gRPC runtime";
    }

    // 起同进程的 muduo_grpc::Server,并在节点实体上挂一条连向它的真实通道。
    void StartLoopbackServerAndChannel()
    {
        const uint16_t port = FindFreeLoopbackPort();
        ASSERT_NE(port, 0) << "no free loopback port";

        muduo_grpc::ServerOptions options;
        options.name = "grpc_runtime_probe";
        options.statsLogInterval = std::chrono::seconds(0);
        server_ = muduo_grpc::Server::Create(loop_, "127.0.0.1", port, options);
        ASSERT_NE(server_, nullptr);
        ASSERT_TRUE(server_->RegisterService(service_));
        server_->Start();

        channel_ = muduo_grpc::Channel::Create(loop_, "127.0.0.1", port);
        ASSERT_NE(channel_, nullptr);
        nodeEntity_ = registry_.create();
        registry_.emplace<MuduoGrpcChannelPtr>(nodeEntity_, channel_);
    }

    // 在 loop 上转到 done() 为真或超时;返回 done() 的最终结果。不能在 loop 自己的回调里调用。
    bool RunLoopUntil(const std::function<bool()> &done, std::chrono::milliseconds timeout)
    {
        if (done())
        {
            return true;
        }
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        const muduo::net::TimerId check = loop_.runEvery(0.005, [this, &done, deadline]
                                                         {
            if (done() || std::chrono::steady_clock::now() >= deadline)
            {
                loop_.quit();
            } });
        loop_.loop();
        loop_.cancel(check);
        return done();
    }

    void RunLoopFor(std::chrono::milliseconds duration)
    {
        RunLoopUntil([] { return false; }, duration);
    }

    // 在 loop 任务里发起一次带 done 的调用(Send 只在 loop 线程上调用;本地完成取的是当前线程的 loop)。
    void SendInLoop(entt::entity target, const std::shared_ptr<Observed> &observed, uint64_t playerId)
    {
        loop_.queueInLoop([this, target, observed, playerId]
        {
            LoadPlayerDataRequest request;
            request.set_player_id(playerId);
            GrpcCallOptions options;
            options.deadline = kLiveness;
            data_service::SendDataServiceLoadPlayerData(
                registry_, target, request,
                [observed](const GrpcCallOutcome &outcome, const LoadPlayerDataResponse *reply)
                {
                    ++observed->calls;
                    observed->code = outcome.code;
                    observed->message = outcome.message;
                    observed->hadReply = reply != nullptr;
                    observed->version = reply != nullptr ? reply->version() : 0;
                },
                options);
        });
    }

    // 声明顺序 = 析构逆序:EventLoop 最先构造、最后析构;service_ 活过 server_。
    muduo::net::EventLoop loop_;
    EchoDataService service_;
    std::shared_ptr<muduo_grpc::Server> server_;
    entt::registry registry_;
    MuduoGrpcChannelPtr channel_;
    entt::entity nodeEntity_{entt::null};
};

// 生成客户端的本地拒绝:节点实体上没有通道,done 以 UNAVAILABLE 完成;官方运行时仍未初始化。
TEST_F(GrpcRuntimeNotInitializedTest, GeneratedClientLocalRejectLeavesRuntimeUninitialized)
{
    const auto nodeWithoutChannel = registry_.create();
    const auto observed = std::make_shared<Observed>();
    SendInLoop(nodeWithoutChannel, observed, 42);
    ASSERT_TRUE(RunLoopUntil([&] { return observed->calls > 0; }, kLiveness));

    EXPECT_EQ(observed->calls, 1);
    EXPECT_EQ(observed->code, kGrpcStatusUnavailable) << observed->message;
    EXPECT_FALSE(observed->hadReply);
    EXPECT_FALSE(IsGrpcRuntimeInitialized()) << "generated client local reject initialized the official gRPC runtime";
}

// 一次真实的 muduo_grpc 调用(生成客户端 → Channel → 同进程 Server → GrpcUnaryCall):OK 应答来自服务端业务;
// 官方运行时仍未初始化。拆除之后 TearDown 再断言一次。
TEST_F(GrpcRuntimeNotInitializedTest, MuduoGrpcLoopbackCallLeavesRuntimeUninitialized)
{
    ASSERT_NO_FATAL_FAILURE(StartLoopbackServerAndChannel());
    const auto observed = std::make_shared<Observed>();
    SendInLoop(nodeEntity_, observed, 7);
    ASSERT_TRUE(RunLoopUntil([&] { return observed->calls > 0; }, kLiveness));

    EXPECT_EQ(observed->calls, 1);
    EXPECT_EQ(observed->code, kGrpcStatusOk) << observed->message;
    EXPECT_TRUE(observed->hadReply);
    EXPECT_EQ(observed->version, 8u) << "reply must come from the server-side business handler";
    EXPECT_EQ(service_.businessRuns, 1);
    EXPECT_FALSE(IsGrpcRuntimeInitialized()) << "muduo_grpc client / server call initialized the official gRPC runtime";
}

} // namespace
