// rpc_client_lifecycle_test — 回归测试,对应事故
//   docs/ops/incident-gate-tcpconnection-dtor-assert-2026-09-13.md §7.1 与 §6.3 的更正
//
// 两条不变量,都围绕"出站节点链路(RpcClient / TcpClient)被拆掉的那一刻":
//
//  1. 握手重试定时器不得延长连接寿命。TryRegisterNodeSession 挂的 0.5s 重试闭包若按值捕获
//     TcpConnectionPtr / RpcClientPtr(旧代码),就是在 muduo Timer 里多存了一份强引用,与
//     tlsRpc.conn 同形:节点在窗口内被摘除时 ~TcpClient 看到 use_count()==2 → 跳过 forceClose
//     → TcpConnection.cc:71 assert。判据:调用前后 conn.use_count() 不变。
//
//  2. ~RpcClient 之后,连接不得再回调进已释放的 RpcClient / RpcChannel。muduo 的
//     TcpClient::newConnection 会把 connectionCallback / messageCallback 拷到 TcpConnection 上;
//     ~TcpClient 只换 closeCallback、forceClose 又一律 queueInLoop,所以 handleClose →
//     connectionCallback_ 必然发生在 ~RpcClient 返回之后。旧代码绑的是裸 this / 裸 channel 指针
//     —— RpcClient::onConnection 的 DOWN 分支写已释放的堆,再 trigger OnConnected2TcpServerEvent。
//     现行修法:回调在 RpcClient::connect() 里只绑 weak_ptr<RpcClient>(消息回调只在取 channel 的那一行
//     lock 它,派发期间由局部强引用保活 channel),对象没了 lock() 失败即返回,没有析构函数、也不需要。
//     出站 RpcChannel 每次连上新建、断开即丢弃(docs/design/muduo-rpc-channel.md §6.1),只存 weak 连接。
//     判据:销毁 RpcClient 并把 loop 转空后,loop 线程的 tlsEcs.dispatcher 上不得再收到该连接的
//     DOWN 事件;channel 随 RpcClient 立即析构;同时对端(server 侧)必须真的看到 DOWN —— 证明
//     forceClose 确实执行了,拆连路径上没有人把 use_count 抬高而重蹈覆辙。
//
// 修复前跑第 2 条是未定义行为(onConnection 在已释放的对象上执行):Debug CRT 下多半表现为
// 计数 +1 的干净失败,也可能直接崩;两者都算红。
//
// 与 rpc_controller_test.cpp 同一工程、同一个 main;这里的回环夹具多了一个真实的 RpcClient。
// RpcChannel 本身的调用 / 回包 / 超时 / 断连契约在 rpc_channel_loopback_test.cpp 与 rpc_chain_test.cpp。

#include <gtest/gtest.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#endif

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <thread>
#include <utility>
#include <google/protobuf/util/json_util.h>
#include "network/rpc_controller.h"
#include "node/system/discovery/service_discovery_manager.h"
#include "proto/common/base/message.pb.h"
#include "table/proto/tip/common_error_tip.pb.h"

#include "muduo/base/CountDownLatch.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/EventLoopThread.h"
#include "muduo/net/InetAddress.h"
#include "muduo/net/TcpConnection.h"
#include "muduo/net/TcpServer.h"

#include "network/rpc_channel.h"                             // RpcChannel::Create / RpcChannelPtr
#include "network/rpc_client.h"                              // RpcClient / RpcClientPtr
#include "network/rpc_connection_event.h"                    // OnConnected2TcpServerEvent
#include "network/rpc_session.h"                             // RpcSession
#include "node/system/node/node_util.h"                      // NodeUtils::RemoveRpcSessionsBoundTo
#include "node/system/registration/registration_manager.h"  // NodeHandshakeManager
#include "thread_context/ecs_context.h"                      // tlsEcs(thread_local)
#include "thread_context/node_context_manager.h"             // tlsNodeContextManager(thread_local)
#include "proto/common/base/common.pb.h"                     // NodeInfo
#include "proto/common/base/node.pb.h"                       // common::base::SceneNodeService

using namespace std::chrono_literals;

// 链接桩(整个测试工程只在这里定义一次)。本文件调用 NodeHandshakeManager::TryRegisterNodeSession,
// 于是把 core.lib 里的 registration_manager.obj 链进来;它的其他方法引用全局 gNode,又把 node.obj 拉进来;
// 而 Node::RegisterHandlers 引用的下面三个注册入口是**各节点可执行工程的生成代码**才定义的
// (gate / scene / battle 各一份),任何 lib 里都没有 —— 不给就 LNK2019 ×3。
// 按 message_id 的全局回包表(InitReply)已随 RpcChannel 改造退场,Node 不再要求它(muduo-rpc-channel.md §9.2)。
// 本测试永远不会构造 Node,给空实现即可。若日后再报同类未解析符号,按同样方式补。
void InitPlayerService() {}
void InitPlayerServiceReplied() {}
void InitServiceHandler() {}

namespace
{

constexpr uint32_t kSceneNodeType = static_cast<uint32_t>(common::base::SceneNodeService);

uint16_t FindFreeLoopbackPort()
{
    const SOCKET s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    EXPECT_NE(s, INVALID_SOCKET);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    EXPECT_EQ(::bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)), 0);
    int len = sizeof(addr);
    EXPECT_EQ(::getsockname(s, reinterpret_cast<sockaddr*>(&addr), &len), 0);
    const uint16_t port = ntohs(addr.sin_port);
    ::closesocket(s);
    return port;
}

// 记录 loop 线程 tlsEcs.dispatcher 上的 OnConnected2TcpServerEvent。
// 只在 loop 线程上 connect / disconnect(dispatcher 是 thread_local 的)。
struct EventCounter
{
    std::atomic<int> up{ 0 };
    std::atomic<int> down{ 0 };
    std::promise<void> firstUp;
    std::atomic<bool> upSignalled{ false };

    void OnEvent(const OnConnected2TcpServerEvent& e)
    {
        if (e.conn_ && e.conn_->connected())
        {
            ++up;
            if (!upSignalled.exchange(true)) firstUp.set_value();
        }
        else
        {
            ++down;
        }
    }
};

class RpcClientLoopback : public ::testing::Test
{
protected:
    void SetUp() override
    {
        loop_.emplace(*loopThread_.startLoop());
        const uint16_t port = FindFreeLoopbackPort();
        ASSERT_NE(port, 0);
        serverAddr_ = muduo::net::InetAddress("127.0.0.1", port);

        RunInLoopAndWait([this, port] {
            server_ = std::make_unique<muduo::net::TcpServer>(
                &Loop(), muduo::net::InetAddress(port, /*loopbackOnly=*/true), "rpc_client_lifecycle_server");
            // server 侧连接只留 weak_ptr(测试 4 要用它构造 RpcSession),不延长它的寿命。
            server_->setConnectionCallback([this](const muduo::net::TcpConnectionPtr& conn) {
                if (conn->connected()) { serverConnWeak_ = conn; return; }
                if (!downSignalled_.exchange(true)) serverDown_.set_value();
            });
            server_->start();
            tlsEcs.dispatcher.sink<OnConnected2TcpServerEvent>().connect<&EventCounter::OnEvent>(events_);
        });
    }

    void TearDown() override
    {
        RunInLoopAndWait([this] {
            tlsEcs.dispatcher.sink<OnConnected2TcpServerEvent>().disconnect<&EventCounter::OnEvent>(events_);
            server_.reset();
        });
    }

    muduo::net::EventLoop& Loop() { return loop_->get(); }

    template <typename F>
    void RunInLoopAndWait(F&& fn)
    {
        muduo::CountDownLatch done(1);
        Loop().runInLoop([&] { fn(); done.countDown(); });
        done.wait();
    }

    // 把 loop 多转几圈,让 forceCloseInLoop / handleClose / connectDestroyed 这些排队的 functor 跑完。
    void PumpLoop(int rounds)
    {
        for (int i = 0; i < rounds; ++i) RunInLoopAndWait([] {});
    }

    // 在 loop 线程上建一个真实的 RpcClient 并等它报 UP。
    RpcClientPtr ConnectClient()
    {
        auto upFuture = events_.firstUp.get_future();
        RpcClientPtr client;
        RunInLoopAndWait([&] {
            client = std::make_shared<RpcClient>(&Loop(), serverAddr_);
            client->connect();
        });
        EXPECT_EQ(upFuture.wait_for(5s), std::future_status::ready) << "RpcClient never reported UP";
        return client;
    }

    // 销毁 RpcClient,把 loop 转空,并要求对端确实看到 DOWN。
    // 返回 (销毁前, 转空后) 的 DOWN 事件计数。
    std::pair<int, int> DestroyClientAndDrain(RpcClientPtr& client)
    {
        auto downFuture = serverDown_.get_future();
        int downBefore = 0;
        RunInLoopAndWait([&] {
            downBefore = events_.down.load();
            client.reset();   // ~RpcClient → ~TcpClient → (unique 时)forceClose 入队
        });
        PumpLoop(3);
        EXPECT_EQ(downFuture.wait_for(5s), std::future_status::ready)
            << "对端没看到 DOWN:forceClose 没有执行 —— 析构路径上又有人把 use_count 抬高了";
        return { downBefore, events_.down.load() };
    }

    // loopThread_ 排第一:最后析构,保证其余成员析构时 loop 仍在。
    muduo::net::EventLoopThread loopThread_;
    // 引用而非裸指针成员(裸指针成员门禁覆盖测试工程);SetUp 里绑定。
    std::optional<std::reference_wrapper<muduo::net::EventLoop>> loop_;
    muduo::net::InetAddress serverAddr_;
    std::unique_ptr<muduo::net::TcpServer> server_;
    EventCounter events_;
    std::weak_ptr<muduo::net::TcpConnection> serverConnWeak_;
    std::promise<void> serverDown_;
    std::atomic<bool> downSignalled_{ false };
};

} // namespace

// ---------------------------------------------------------------------------
// 1. 握手重试定时器(registration_manager.cpp TryRegisterNodeSession)
// ---------------------------------------------------------------------------

TEST_F(RpcClientLoopback, HandshakeRetryTimerDoesNotRetainTheConnection)
{
    RpcClientPtr client = ConnectClient();
    ASSERT_TRUE(client && client->connected());

    long before = -1, after = -1, afterDestroy = -1;
    RunInLoopAndWait([&] {
        // tlsNodeContextManager 是 thread_local:实体必须建在 loop 线程的那份注册表里,
        // TryRegisterNodeSession 也在这个线程上按连接指针匹配到它。
        entt::registry& registry = tlsNodeContextManager.GetRegistry(kSceneNodeType);
        const entt::entity node = registry.create();
        registry.emplace<RpcClientPtr>(node, client);
        registry.emplace<NodeInfo>(node);   // 内容无关:匹配只看 GetConnection().get()

        const muduo::net::TcpConnectionPtr conn = client->GetConnection();
        if (!conn) { ADD_FAILURE() << "client has no connection"; return; }

        before = conn.use_count();
        NodeHandshakeManager{}.TryRegisterNodeSession(kSceneNodeType, conn);
        after = conn.use_count();

        // 立刻销毁实体:~TimerTaskComp → Cancel → cancelInLoop 同步 delete 定时器与闭包。
        // 0.5s 后不会再有回调去碰 GetNodeInfo() / gNode(单测进程里没有 Node)。
        registry.destroy(node);
        afterDestroy = conn.use_count();
    });

    EXPECT_EQ(before, after)
        << "TryRegisterNodeSession 之后连接多出 " << (after - before)
        << " 个强引用 —— 重试闭包按值捕获了 TcpConnectionPtr(旧写法),与 tlsRpc.conn 同形";
    EXPECT_EQ(before, afterDestroy) << "实体销毁后引用计数没有回到基线";

    const auto [downBefore, downAfter] = DestroyClientAndDrain(client);
    (void)downBefore; (void)downAfter;   // 这里只要求对端看到 DOWN(在 DestroyClientAndDrain 里断言)
}

TEST_F(RpcClientLoopback, InboundHandshakeRejectsGrpcOnlyTypesBeforeTouchingAnyNodeRegistry)
{
    auto client = ConnectClient();
    RunInLoopAndWait([&]
    {
        const auto conn = client->GetConnection();
        ASSERT_NE(conn, nullptr);
        RpcController context(conn);
        for (const uint32_t type : {uint32_t(common::base::SceneManagerNodeService),
                                   uint32_t(common::base::DataServiceNodeService),
                                   uint32_t(common::base::EtcdNodeService), uint32_t(0), UINT32_MAX})
        {
            NodeHandshakeRequest request;
            request.mutable_self_node()->set_node_type(type);
            request.mutable_self_node()->set_node_uuid("must-not-register");
            NodeHandshakeResponse response;
            // 测试不构造 Node；应在任何 gNode/registry 访问前明确拒绝，而不是误入注册流程。
            NodeHandshakeManager{}.OnNodeHandshake(context, request, response);
            EXPECT_EQ(response.error_message().id(), kFailedToRegisterTheNode);
            if (auto *registry = tlsNodeContextManager.TryGetRegistry(type))
            {
                for (const auto entity : registry->view<NodeInfo>())
                {
                    EXPECT_NE(registry->get<NodeInfo>(entity).node_uuid(), "must-not-register");
                }
            }
        }
    });
    DestroyClientAndDrain(client);
}

TEST(ServiceDiscoveryTypeBoundary, MismatchedJsonTypeIsRejectedBeforeSnapshotOrConnectionSideEffects)
{
    ServiceDiscoveryManager manager;
    NodeInfo node;
    node.set_node_type(common::base::SceneManagerNodeService);
    node.set_node_uuid("wrong-prefix-type");
    std::string json;
    ASSERT_TRUE(google::protobuf::util::MessageToJsonString(node, &json).ok());
    // 无 Node 实例时也能验证真正入口：若未先拒绝，取消 pending removal/连接阶段会访问空 gNode。
    manager.AddServiceNode(json, common::base::DataServiceNodeService);
    EXPECT_TRUE(manager.ServiceNodes().NodesOfType(common::base::DataServiceNodeService).empty());
    EXPECT_TRUE(manager.ServiceNodes().NodesOfType(common::base::SceneManagerNodeService).empty());
}

// ---------------------------------------------------------------------------
// 2. ~RpcClient 之后不得再回调进已释放对象(rpc_client.h:connect() 里的 weak_ptr 绑定)
// ---------------------------------------------------------------------------

TEST_F(RpcClientLoopback, DestroyingRpcClientDetachesCallbacksBeforeCloseRuns)
{
    RpcClientPtr client = ConnectClient();
    ASSERT_TRUE(client && client->connected());
    ASSERT_EQ(events_.up.load(), 1);

    std::weak_ptr<RpcChannel> channelWeak;
    RunInLoopAndWait([&] { channelWeak = client->channel(); });
    ASSERT_FALSE(channelWeak.expired()) << "a connected RpcClient must have a channel for its current connection";

    const auto [downBefore, downAfter] = DestroyClientAndDrain(client);
    EXPECT_EQ(downBefore, downAfter)
        << "~RpcClient 之后连接仍回调进已释放的 RpcClient::onConnection(use-after-free):"
           " DOWN 事件多了 " << (downAfter - downBefore) << " 个";
    EXPECT_TRUE(channelWeak.expired())
        << "the channel outlived its destroyed RpcClient (does a message callback or the timeout tick hold it strongly?)";
}

// ---------------------------------------------------------------------------
// 3. 同 uuid 重注册路径(node_connector.cpp ConnectToTcpNode):旧 RpcClient 的最后一根引用
//    由 registry.destroy(existingEntity) 释放。这里曾把旧 client push 进从不排空的
//    Node::disconnectedClientList 来"延迟关闭"(为绕开本事故加的兜底),现已移除。本用例锁定
//    移除后的行为:实体销毁 ⇒ RpcClient 立即析构 ⇒ 无 UAF 回调 ⇒ 对端看到 DOWN。
//    ConnectToTcpNode 本身依赖 gNode,单测里不构造 Node,因此只复刻它对实体做的事。
// ---------------------------------------------------------------------------

TEST_F(RpcClientLoopback, DestroyingNodeEntityReleasesItsRpcClientCleanly)
{
    RpcClientPtr client = ConnectClient();
    ASSERT_TRUE(client && client->connected());

    auto downFuture = serverDown_.get_future();
    int downBefore = 0;
    RunInLoopAndWait([&] {
        entt::registry& registry = tlsNodeContextManager.GetRegistry(kSceneNodeType);
        const entt::entity node = registry.create();
        registry.emplace<RpcClientPtr>(node, client);
        registry.emplace<NodeInfo>(node);
        client.reset();                 // 组件现在是唯一持有者,与生产一致
        downBefore = events_.down.load();
        registry.destroy(node);         // == node_connector.cpp 的 targetRegistry.destroy(existingEntity)
    });
    PumpLoop(3);

    EXPECT_EQ(downFuture.wait_for(5s), std::future_status::ready)
        << "对端没看到 DOWN:实体销毁没有让旧连接 forceClose(是否又有人把 RpcClient / conn 存起来了?)";
    EXPECT_EQ(downBefore, events_.down.load())
        << "实体销毁后连接仍回调进已释放的 RpcClient(use-after-free)";
}

// ---------------------------------------------------------------------------
// 4. 入站链路 DOWN 时摘 RpcSession(rpc_server.cc DOWN 分支 → NodeUtils::RemoveRpcSessionsBoundTo)
//    RpcSession::connection 已是 weak_ptr,不摘不会钉住连接;摘是为了让路由方立刻看到"不可达",
//    而不是留一个 lock() 永远失败的空壳等到 etcd 键过期(≤180s)。
//    直接测抽出来的函数:按连接指针精确匹配、跨所有节点类型注册表、不误伤别的连接。
//    RpcSession 的构造函数从 conn 的 context 里 any_cast RpcChannelPtr(生产由 RpcServer::onConnection
//    设置:每条入站连接一个 channel,归 context 所有),这里手工挂同样的 context。RpcSession 只存
//    weak_ptr<RpcChannel>:清掉 context(== RpcServer 的 DOWN 分支)后 channel 必须立即析构,
//    组件还挂着也钉不住它。
// ---------------------------------------------------------------------------

TEST_F(RpcClientLoopback, RemoveRpcSessionsBoundToDetachesOnlyThatConnection)
{
    RpcClientPtr client = ConnectClient();
    ASSERT_TRUE(client && client->connected());

    std::size_t removed = 0;
    bool aGone = false, bStays = false, cGone = false;
    bool sessionReachedChannel = false;
    bool channelGoneWithSessionsAttached = false;
    bool sessionReportsUnreachable = false;
    RunInLoopAndWait([&] {
        const muduo::net::TcpConnectionPtr serverConn = serverConnWeak_.lock();
        const muduo::net::TcpConnectionPtr clientConn = client->GetConnection();
        if (!serverConn || !clientConn) { ADD_FAILURE() << "missing connections"; return; }
        std::weak_ptr<RpcChannel> serverChannelWeak;
        {
            const RpcChannelPtr serverChannel = RpcChannel::Create(Loop(), serverConn, RpcServiceMap{});
            serverChannelWeak = serverChannel;
            serverConn->setContext(serverChannel);   // context 成为唯一的强持有者
        }

        entt::registry& sceneReg = tlsNodeContextManager.GetRegistry(kSceneNodeType);
        entt::registry& gateReg  = tlsNodeContextManager.GetRegistry(static_cast<uint32_t>(common::base::GateNodeService));
        const entt::entity a = sceneReg.create();   // 绑 server 侧连接        → 应被摘
        const entt::entity b = sceneReg.create();   // 绑另一条连接            → 应保留
        const entt::entity c = gateReg.create();    // 另一个注册表、同一连接  → 应被摘
        sceneReg.emplace<RpcSession>(a, RpcSession{ serverConn, "uuid-a" });
        // 出站侧连接的 context 是空的(RpcClient 自持 channel,不用 context):这个 session 没有 channel,
        // 但连接仍活着 —— 摘不摘只看连接,不看 channel。
        sceneReg.emplace<RpcSession>(b, RpcSession{ clientConn, "uuid-b" });
        gateReg.emplace<RpcSession>(c, RpcSession{ serverConn, "uuid-c" });
        sessionReachedChannel = sceneReg.get<RpcSession>(a).IsConnected();

        serverConn->setContext(RpcChannelPtr());
        channelGoneWithSessionsAttached = serverChannelWeak.expired();
        sessionReportsUnreachable = !sceneReg.get<RpcSession>(a).IsConnected();

        removed = NodeUtils::RemoveRpcSessionsBoundTo(serverConn);
        aGone  = sceneReg.try_get<RpcSession>(a) == nullptr;
        bStays = sceneReg.try_get<RpcSession>(b) != nullptr;
        cGone  = gateReg.try_get<RpcSession>(c) == nullptr;

        sceneReg.destroy(a); sceneReg.destroy(b); gateReg.destroy(c);
    });
    EXPECT_TRUE(sessionReachedChannel) << "RpcSession did not pick up the inbound channel from the connection context";
    EXPECT_TRUE(channelGoneWithSessionsAttached)
        << "the channel survived clearing the context: RpcSession holds it strongly (must hold weak_ptr<RpcChannel> only)";
    EXPECT_TRUE(sessionReportsUnreachable) << "RpcSession::IsConnected() still reports reachable after its channel died";
    EXPECT_EQ(removed, 2u);
    EXPECT_TRUE(aGone)  << "绑在断开连接上的 RpcSession 没被摘";
    EXPECT_TRUE(bStays) << "误伤了绑在别的连接上的 RpcSession";
    EXPECT_TRUE(cGone)  << "另一个节点类型注册表里的同连接 RpcSession 没被摘";

    // 上面的临时引用都已释放,常规拆连仍须干净:对端看到 DOWN、无 UAF 回调。
    const auto [downBefore, downAfter] = DestroyClientAndDrain(client);
    EXPECT_EQ(downBefore, downAfter);
}

// ---------------------------------------------------------------------------
// 5. 出站 channel 不得钉住已断开的连接
//    旧写法:RpcClient 终身复用一个 channel 且强持当前连接,DOWN 不清,要等重连成功被新连接覆盖才放 ——
//    对端没了、重连不成功,死连接和它的 fd 就一直被钉着。现在出站 channel 每次连上新建、只存 weak 连接,
//    DOWN 时在途调用以 kDisconnected 失败后 channel 即被丢弃(rpc_client.h onConnection)。
//    这里直接销毁 server 让重连必失败:client 侧收到 DOWN 且 loop 转空后,RpcClient 不再有 channel,
//    旧 channel 与那条连接对象都必须已经析构(weak_ptr expired)。
// ---------------------------------------------------------------------------

TEST_F(RpcClientLoopback, ClientChannelDoesNotPinDeadConnection)
{
    RpcClientPtr client = ConnectClient();
    ASSERT_TRUE(client && client->connected());

    std::weak_ptr<muduo::net::TcpConnection> clientConnWeak;
    std::weak_ptr<RpcChannel> channelWeak;
    RunInLoopAndWait([&] {
        clientConnWeak = client->GetConnection();
        channelWeak = client->channel();
        server_.reset();   // ~TcpServer → 对端所有连接 connectDestroyed → client 收到 DOWN;之后重连必失败
    });
    ASSERT_FALSE(channelWeak.expired()) << "a connected RpcClient must have a channel for its current connection";
    const auto deadline = std::chrono::steady_clock::now() + 5s;   // 只作活性上限,判据是 DOWN 事件本身
    while (events_.down.load() == 0 && std::chrono::steady_clock::now() < deadline)
    {
        std::this_thread::sleep_for(10ms);
    }
    ASSERT_GE(events_.down.load(), 1) << "client 没有收到 DOWN";
    PumpLoop(3);

    bool stillConnected = true;
    bool stillHasChannel = true;
    RunInLoopAndWait([&] {
        stillConnected = client->connected();
        stillHasChannel = client->channel() != nullptr;
    });
    EXPECT_FALSE(stillConnected) << "RpcClient::connected() is still true after DOWN";
    EXPECT_FALSE(stillHasChannel) << "the outbound channel is per connection and must be dropped on DOWN";
    EXPECT_TRUE(channelWeak.expired()) << "the old connection's channel survived DOWN and a drained loop: someone still holds it strongly";
    EXPECT_TRUE(clientConnWeak.expired())
        << "连接已 DOWN 且 loop 已转空,client 侧连接对象仍活着 —— 有人(RpcChannel::connection_?)还攥着强引用";

    RunInLoopAndWait([&] { client.reset(); });   // 此时 client_.connection() 已空,~TcpClient 走 connector 分支
    PumpLoop(2);
}
