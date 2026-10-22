#ifdef WIN32

// Windows 纯 FIN 可能只产生 POLLHUP；真实 Channel 与 Hiredis 必须让读处理观察 EOF。
#include <winsock2.h>
#include <ws2tcpip.h>

#include <chrono>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "muduo_test_loop.h"
#include "contrib/hiredis/Hiredis.h"
#include "muduo/base/Timestamp.h"
#include "muduo/net/Channel.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/InetAddress.h"
#include "muduo/net/SocketsOps.h"

#include <poll.h>

namespace
{
using namespace std::chrono_literals;

struct SocketLease
{
    SOCKET value{INVALID_SOCKET};
    SocketLease() = default;
    SocketLease(const SocketLease &) = delete;
    SocketLease &operator=(const SocketLease &) = delete;
    ~SocketLease()
    {
        if (value != INVALID_SOCKET)
        {
            ::closesocket(value);
        }
    }
};

bool ListenLoopback(SocketLease &listener, uint16_t &port)
{
    listener.value = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener.value == INVALID_SOCKET)
    {
        return false;
    }
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;
    int size = sizeof(address);
    if (::bind(listener.value, reinterpret_cast<sockaddr *>(&address), size) != 0 ||
        ::getsockname(listener.value, reinterpret_cast<sockaddr *>(&address), &size) != 0 ||
        ::listen(listener.value, 1) != 0)
    {
        return false;
    }
    port = ntohs(address.sin_port);
    return port != 0;
}

bool WaitReadableOrHangup(SOCKET socket)
{
    WSAPOLLFD descriptor{};
    descriptor.fd = socket;
    descriptor.events = POLLIN;
    const int result = ::WSAPoll(&descriptor, 1, 1500);
    return result == 1 && (descriptor.revents & (POLLIN | POLLHUP)) != 0;
}

bool OpenPair(SocketLease &receiver, SocketLease &peer)
{
    SocketLease listener;
    uint16_t port = 0;
    if (!ListenLoopback(listener, port))
    {
        return false;
    }
    receiver.value = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (receiver.value == INVALID_SOCKET)
    {
        return false;
    }
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(port);
    if (::connect(receiver.value, reinterpret_cast<sockaddr *>(&address), sizeof(address)) != 0 ||
        !WaitReadableOrHangup(listener.value))
    {
        return false;
    }
    peer.value = ::accept(listener.value, nullptr, nullptr);
    return peer.value != INVALID_SOCKET;
}

class WindowsSocketsReady : public ::testing::Test
{
protected:
    void SetUp() override
    {
        static const int status = [] {
            WSADATA data{};
            return ::WSAStartup(MAKEWORD(2, 2), &data);
        }();
        ASSERT_EQ(status, 0);
    }
};

using HupChannelContract = WindowsSocketsReady;
using HiredisPeerFin = WindowsSocketsReady;
using WindowsAcceptedSocketContract = WindowsSocketsReady;

TEST_F(HupChannelContract, HupOnlyReadWriteConsumerReceivesRealEofExactlyOnce)
{
    muduo::net::EventLoop loop;
    SocketLease receiver;
    SocketLease peer;
    ASSERT_TRUE(OpenPair(receiver, peer));
    ASSERT_LE(receiver.value, static_cast<SOCKET>((std::numeric_limits<int>::max)()));
    ASSERT_EQ(::shutdown(peer.value, SD_SEND), 0);
    ASSERT_TRUE(WaitReadableOrHangup(receiver.value));

    int reads = 0;
    int writes = 0;
    std::optional<int> received;
    muduo::net::Channel channel(&loop, static_cast<int>(receiver.value));
    channel.doNotLogHup();
    channel.setReadCallback([&](muduo::Timestamp) {
        ++reads;
        char byte{};
        received = ::recv(receiver.value, &byte, 1, 0);
    });
    channel.setWriteCallback([&] { ++writes; });
    channel.set_revents(POLLHUP);
    channel.handleEvent(muduo::Timestamp::now());

    EXPECT_EQ(reads, 1) << "纯 HUP 必须让只设读写回调的消费者观察 EOF";
    EXPECT_EQ(received, std::optional<int>{0});
    EXPECT_EQ(writes, 0);
}

TEST_F(HupChannelContract, HupAndWriteDoesNotWriteAfterReadOnlyConsumerCloses)
{
    muduo::net::EventLoop loop;
    int reads = 0;
    int writes = 0;
    muduo::net::Channel channel(&loop, -1);
    channel.doNotLogHup();
    channel.setReadCallback([&](muduo::Timestamp) { ++reads; });
    channel.setWriteCallback([&] { ++writes; });
    channel.set_revents(POLLHUP | POLLOUT);
    channel.handleEvent(muduo::Timestamp::now());
    EXPECT_EQ(reads, 1);
    EXPECT_EQ(writes, 0) << "EOF 读回调可能已释放 hiredis context";
}

TEST_F(HupChannelContract, CloseConsumerStopsBeforeEveryOtherCallback)
{
    muduo::net::EventLoop loop;
    std::vector<std::string> calls;
    muduo::net::Channel channel(&loop, -1);
    channel.doNotLogHup();
    channel.setCloseCallback([&] { calls.emplace_back("close"); });
    channel.setErrorCallback([&] { calls.emplace_back("error"); });
    channel.setReadCallback([&](muduo::Timestamp) { calls.emplace_back("read"); });
    channel.setWriteCallback([&] { calls.emplace_back("write"); });
    channel.set_revents(POLLHUP | POLLERR | POLLOUT);
    channel.handleEvent(muduo::Timestamp::now());
    EXPECT_EQ(calls, (std::vector<std::string>{"close"}));
}

TEST_F(HupChannelContract, ConnectorShapeHupErrorWriteReportsErrorOnceBeforeWrite)
{
    muduo::net::EventLoop loop;
    std::vector<std::string> calls;
    muduo::net::Channel channel(&loop, -1);
    channel.doNotLogHup();
    channel.setErrorCallback([&] { calls.emplace_back("error"); });
    channel.setWriteCallback([&] { calls.emplace_back("write"); });
    channel.set_revents(POLLHUP | POLLERR | POLLOUT);
    channel.handleEvent(muduo::Timestamp::now());
    EXPECT_EQ(calls, (std::vector<std::string>{"error", "write"}));
}

TEST_F(HupChannelContract, ConnectorShapeHupWithoutErrorBitStillReportsErrorOnce)
{
    muduo::net::EventLoop loop;
    int errors = 0;
    int writes = 0;
    muduo::net::Channel channel(&loop, -1);
    channel.doNotLogHup();
    channel.setErrorCallback([&] { ++errors; });
    channel.setWriteCallback([&] { ++writes; });
    channel.set_revents(POLLHUP);
    channel.handleEvent(muduo::Timestamp::now());
    EXPECT_EQ(errors, 1);
    EXPECT_EQ(writes, 0);
}

TEST_F(HupChannelContract, ReadableAndHupKeepsExistingReadDispatch)
{
    muduo::net::EventLoop loop;
    int reads = 0;
    int closes = 0;
    muduo::net::Channel channel(&loop, -1);
    channel.doNotLogHup();
    channel.setReadCallback([&](muduo::Timestamp) { ++reads; });
    channel.setCloseCallback([&] { ++closes; });
    channel.set_revents(POLLIN | POLLHUP);
    channel.handleEvent(muduo::Timestamp::now());
    EXPECT_EQ(reads, 1);
    EXPECT_EQ(closes, 0);
}

TEST_F(HupChannelContract, HupWithoutCallbacksReturnsAndCanBeDestroyed)
{
    muduo::net::EventLoop loop;
    muduo::net::Channel channel(&loop, -1);
    channel.doNotLogHup();
    channel.set_revents(POLLHUP);
    channel.handleEvent(muduo::Timestamp::now());
}

// 不需要 Redis 服务：原生回环端只 accept、从不回 RESP，再 shutdown(SD_SEND)。
// 客户端走真实 Hiredis -> Channel -> WSAPoll；挂起 PING 须以空应答恰好完成一次。
TEST_F(HiredisPeerFin, PureFinDisconnectsAndCompletesPendingCommandBeforeTimeout)
{
    struct Observations
    {
        int connects{0};
        int connectStatus{12345};
        int disconnects{0};
        int disconnectStatus{12345};
        int commandResult{12345};
        int nullReplies{0};
        int unexpectedReplies{0};
        int shutdownResult{12345};
    } observed;
    muduo::net::EventLoop loop;
    SocketLease listener;
    SocketLease peer;
    uint16_t port = 0;
    ASSERT_TRUE(ListenLoopback(listener, port));
    auto redis = hiredis::Hiredis::Create(&loop, muduo::net::InetAddress("127.0.0.1", port));
    // 夹具对象和回调同寿命；回调执行结束并排空后才销毁，见 AGENTS.md §11.7。
    redis->setConnectCallback([&](hiredis::Hiredis *client, int status) {
        ++observed.connects;
        observed.connectStatus = status;
        if (status != REDIS_OK)
        {
            return;
        }
        observed.commandResult = client->command([&](hiredis::Hiredis *, redisReply *reply) {
            if (reply == nullptr)
            {
                ++observed.nullReplies;
            }
            else
            {
                ++observed.unexpectedReplies;
            }
        }, "PING");
        loop.queueInLoop([&] { observed.shutdownResult = ::shutdown(peer.value, SD_SEND); });
    });
    redis->setDisconnectCallback([&](hiredis::Hiredis *, int status) {
        ++observed.disconnects;
        observed.disconnectStatus = status;
    });
    redis->connect();
    ASSERT_TRUE(WaitReadableOrHangup(listener.value));
    peer.value = ::accept(listener.value, nullptr, nullptr);
    ASSERT_NE(peer.value, INVALID_SOCKET);

    const bool disconnected = muduo_grpc_test::RunLoopUntil(loop, [&] { return observed.disconnects != 0; }, 1500ms);
    // 先检查活体客户端的状态；析构触发的 null-reply 不能冒充正常 EOF 路径成功。
    EXPECT_EQ(observed.connects, 1);
    EXPECT_EQ(observed.connectStatus, REDIS_OK);
    EXPECT_EQ(observed.commandResult, REDIS_OK);
    EXPECT_EQ(observed.shutdownResult, 0);
    EXPECT_TRUE(disconnected) << "纯 FIN 应触发真实 Hiredis 的 disconnectCallback";
    EXPECT_EQ(observed.disconnects, 1);
    EXPECT_EQ(observed.disconnectStatus, REDIS_ERR);
    EXPECT_EQ(observed.nullReplies, 1);
    EXPECT_EQ(observed.unexpectedReplies, 0);
    EXPECT_FALSE(redis->connected());

    redis.reset();
    muduo_grpc_test::RunLoopFor(loop, 30ms); // 清掉 removeChannel 排队保留的 Channel。
}
// 旧适配器会把 accept 到的连接改成阻塞，空读将挂住整个测试线程。
// 此单例由独立测试进程的 5 秒上限兜底；正确行为是直接返回 WSAEWOULDBLOCK。
TEST_F(WindowsAcceptedSocketContract, AcceptedConnectionReturnsWouldBlockWithoutData)
{
    SocketLease listener;
    SocketLease client;
    SocketLease accepted;
    uint16_t port = 0;
    ASSERT_TRUE(ListenLoopback(listener, port));
    ASSERT_LE(listener.value, static_cast<SOCKET>((std::numeric_limits<int>::max)()));

    client.value = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    ASSERT_NE(client.value, INVALID_SOCKET);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(port);
    ASSERT_EQ(::connect(client.value, reinterpret_cast<sockaddr *>(&address), sizeof(address)), 0);

    sockaddr_in6 peerAddress{};
    const int acceptedFd = muduo::net::sockets::accept(static_cast<int>(listener.value), &peerAddress);
    ASSERT_GE(acceptedFd, 0);
    accepted.value = static_cast<SOCKET>(acceptedFd);

    char byte{};
    const int received = ::recv(accepted.value, &byte, 1, 0);
    const int socketError = received == SOCKET_ERROR ? ::WSAGetLastError() : 0;
    EXPECT_EQ(received, SOCKET_ERROR);
    EXPECT_EQ(socketError, WSAEWOULDBLOCK);
}

} // namespace

#endif // WIN32
