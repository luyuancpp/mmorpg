// redis_manager_auth_test —— 回归测试,对应 docs/handoff/global-singletons-p0-findings-20261014.md P0-A
// 「C++ 节点连 zone Redis 不发 AUTH」。
//
// 被测契约见 cpp/libs/engine/thread_context/redis_manager.h 中 Connect 的注释(fail-closed):
//  1. 口令为空:不发 AUTH,连上即对业务可见,行为与改动前相同;
//  2. 口令非空:每条新连接(首次建连与每次重连)上的第一条命令就是 AUTH;只有 AUTH 回 +OK 后连接才对业务可见、
//     重连回调才被调用,所以回调里发出的命令排在 AUTH 之后;
//  3. AUTH 被拒或一个重连间隔内没有应答:连接不对业务可见(GetZoneRedis() 非空但 connected()==false)、
//     不调重连回调、握手连接上不出现 AUTH 以外的命令,并在回调栈外重建握手连接重试;
//  4. 日志里永远不出现口令原文,服务端回显口令时只保留错误码、其余遮掉;
//  5. 握手中 Shutdown:安静释放,不回调、不重连;
//  6. 重连间隔非正数时回落到默认值并打 WARN。
//
// 做法:被测的 RedisManager(测试自建,不用 tlsRedis)与假 Redis(muduo TcpServer)跑在同一个 loop 线程上。
// 假 Redis 做最小 RESP 解析,按连接记下收到的命令序列,按用例配置对 AUTH 的应答。
// 日志经 muduo::Logger::setOutput 截获:在 loop 线程启动前装上、join 之后卸下,不与 loop 线程竞争全局变量。
// 测试线程等 loop 线程一律用带截止时间的 future(单调时钟),不做无限期等待。
//
// 与 rpc_controller_test.cpp 同一工程、同一个 main(main 把日志级别设为 WARN,本夹具临时调到 INFO);
// InitReply 等链接桩已由 rpc_client_lifecycle_test.cpp 定义,这里不要重复定义。

#include <gtest/gtest.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#endif

#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#include "muduo/base/Logging.h"
#include "muduo/base/Timestamp.h"
#include "muduo/net/Buffer.h"
#include "muduo/net/Callbacks.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/EventLoopThread.h"
#include "muduo/net/InetAddress.h"
#include "muduo/net/TcpConnection.h"
#include "muduo/net/TcpServer.h"
#include "muduo/contrib/hiredis/Hiredis.h"   // 它包含 <hiredis/hiredis.h>,提供 REDIS_OK / redisReply

#include "thread_context/redis_manager.h"

using namespace std::chrono_literals;

namespace
{

// 测试专用的假口令。断言「日志里没有口令原文」就是在找这个串。
constexpr char kTestPassword[] = "p0a-test-pass-01";

// 注入的重连间隔(同时是 AUTH 握手截止):远小于生产默认的 3s,让「断线 / 被拒 / 超时后重建」的用例不必干等。
constexpr double kReconnectIntervalSec = 0.05;
// 不该发生重建的用例用它:整个用例期间截止定时器都不会到点,结果不依赖回环上的应答快慢。
constexpr double kLongReconnectIntervalSec = 30.0;
// 等「应该发生」的事(建连、应答、重连)的上限。
constexpr std::chrono::milliseconds kSettleTimeout = 5s;
// 断言「不该发生」的事(重连、新连接)时观察的窗口:约 10 个重连周期。
constexpr std::chrono::milliseconds kQuietWindow = 500ms;
constexpr std::chrono::milliseconds kPollInterval = 5ms;

// ---------------------------------------------------------------------------
// 日志截获。muduo 的输出函数是函数指针,状态只能放在本 TU 的全局里,用互斥锁保护。
// ---------------------------------------------------------------------------

std::mutex gLogMutex;
std::string gLogText;

void CaptureLogOutput(const char* msg, int len)
{
    std::lock_guard<std::mutex> lock(gLogMutex);
    gLogText.append(msg, static_cast<std::size_t>(len));
}

// muduo 的默认输出函数(Logging.cc 里的 defaultOutput)在匿名作用域里,不可取址;恢复成等价实现。
void StdoutLogOutput(const char* msg, int len)
{
    std::fwrite(msg, 1, static_cast<std::size_t>(len), stdout);
}

void ClearCapturedLog()
{
    std::lock_guard<std::mutex> lock(gLogMutex);
    gLogText.clear();
}

std::string CapturedLog()
{
    std::lock_guard<std::mutex> lock(gLogMutex);
    return gLogText;
}

bool Contains(std::string_view text, std::string_view needle)
{
    return text.find(needle) != std::string_view::npos;
}

std::size_t CountOccurrences(std::string_view text, std::string_view needle)
{
    std::size_t count = 0;
    for (std::size_t pos = text.find(needle); pos != std::string_view::npos; pos = text.find(needle, pos + needle.size()))
    {
        ++count;
    }
    return count;
}

// 失败信息里要打印日志或命令时先把测试口令换成 <pw>:测试输出同样不得出现口令原文(AGENTS.md §11.3)。
std::string MaskPassword(std::string text)
{
    constexpr std::string_view kSecret(kTestPassword);
    constexpr std::string_view kMask("<pw>");
    for (std::size_t pos = text.find(kSecret); pos != std::string::npos; pos = text.find(kSecret, pos + kMask.size()))
    {
        text.replace(pos, kSecret.size(), kMask);
    }
    return text;
}

// ---------------------------------------------------------------------------
// 最小 RESP 解析:只认 hiredis 发命令用的多条批量格式 "*N\r\n" + N 个 "$len\r\n<bytes>\r\n"。
// ---------------------------------------------------------------------------

using RespCommand = std::vector<std::string>;

enum class ParseResult
{
    kIncomplete,
    kParsed,
    kMalformed,
};

// 读一行 "<prefix><非负十进制>\r\n";成功时把 pos 推到 \r\n 之后。
ParseResult ReadLengthLine(std::string_view data, std::size_t& pos, char prefix, std::size_t& value)
{
    if (pos >= data.size())
    {
        return ParseResult::kIncomplete;
    }
    if (data[pos] != prefix)
    {
        return ParseResult::kMalformed;
    }
    const std::size_t crlf = data.find("\r\n", pos + 1);
    if (crlf == std::string_view::npos)
    {
        return ParseResult::kIncomplete;
    }
    const char* first = data.data() + pos + 1;
    const char* last = data.data() + crlf;
    const std::from_chars_result parsed = std::from_chars(first, last, value);
    if (parsed.ec != std::errc() || parsed.ptr != last)
    {
        return ParseResult::kMalformed;
    }
    pos = crlf + 2;
    return ParseResult::kParsed;
}

// 从 data 开头解析一条命令;成功时 consumed 是这条命令占用的字节数。
ParseResult ParseCommand(std::string_view data, std::size_t& consumed, RespCommand& out)
{
    std::size_t pos = 0;
    std::size_t argc = 0;
    if (const ParseResult r = ReadLengthLine(data, pos, '*', argc); r != ParseResult::kParsed)
    {
        return r;
    }
    RespCommand args;
    for (std::size_t i = 0; i < argc; ++i)
    {
        std::size_t len = 0;
        if (const ParseResult r = ReadLengthLine(data, pos, '$', len); r != ParseResult::kParsed)
        {
            return r;
        }
        const std::size_t remaining = data.size() - pos;
        if (len > remaining || remaining - len < 2)
        {
            return ParseResult::kIncomplete;
        }
        if (data.compare(pos + len, 2, "\r\n") != 0)
        {
            return ParseResult::kMalformed;
        }
        args.emplace_back(data.substr(pos, len));
        pos += len + 2;
    }
    consumed = pos;
    out = std::move(args);
    return ParseResult::kParsed;
}

// 命令名转大写(只处理 ASCII),用于不区分大小写地比较 AUTH / PING。
std::string VerbOf(const RespCommand& cmd)
{
    std::string verb = cmd.empty() ? std::string() : cmd.front();
    for (char& c : verb)
    {
        if (c >= 'a' && c <= 'z')
        {
            c = static_cast<char>(c - 'a' + 'A');
        }
    }
    return verb;
}

// 把一条命令写成 "AUTH <pw>" 这样的文本:等于测试口令的参数显示成 <pw>。
// 于是 EXPECT_EQ(..., "AUTH <pw>") 既证明口令按原样作为单个参数发出,失败信息里又不出现原文。
std::string DescribeCommand(const RespCommand& cmd)
{
    std::string text;
    for (const std::string& arg : cmd)
    {
        if (!text.empty())
        {
            text += ' ';
        }
        text += arg;
    }
    return MaskPassword(std::move(text));
}

std::string DescribeCommands(const std::vector<RespCommand>& cmds)
{
    std::string text;
    for (const RespCommand& cmd : cmds)
    {
        if (!text.empty())
        {
            text += " | ";
        }
        text += DescribeCommand(cmd);
    }
    return text;
}

// 同 hiredis_command_lifecycle_test.cpp;各自在本 TU 的匿名命名空间里,不构成 ODR 冲突。
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

// 假 Redis 对 AUTH 的应答方式。
enum class AuthReplyMode
{
    kOk,            // +OK
    kWrongPass,     // -WRONGPASS invalid username-password pair(Redis 6+ 口令错时的原文)
    kEchoPassword,  // -ERR bad password <收到的口令>:模拟会把口令回显进错误文本的代理
    kSwallow,       // 压住不回;此后这条连接上的命令一律不回(真 Redis 按序应答,前一条不回后面都等着)
};

// 假 Redis 眼里的一条连接。
struct FakeConnection
{
    std::vector<RespCommand> commands;   // 按到达顺序记录
    bool stalled = false;                // 已有一条 AUTH 被压住
};

// 测试线程经 RunInLoopAndWait 拿到的一份状态拷贝。
struct Snapshot
{
    std::vector<FakeConnection> connections;   // 下标 = 连接建立的先后
    int serverDowns = 0;
    int malformedFrames = 0;
    int reconnectCalls = 0;
    int pingReplies = 0;
    int pingNullReplies = 0;
    int pingSendFailures = 0;
    bool zoneRedisPresent = false;     // GetZoneRedis() 非空
    bool zoneRedisConnected = false;   // GetZoneRedis() 非空且 connected():即对业务可用
};

class RedisManagerAuth : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // 日志级别与输出函数都是 muduo 的普通全局变量:在 loop 线程启动前改、在它 join 之后改回,
        // 线程创建与 join 提供 happens-before,不与 loop 线程竞争。
        savedLogLevel_ = muduo::Logger::logLevel();
        muduo::Logger::setLogLevel(muduo::Logger::INFO);   // "Redis AUTH ok to" 是 INFO
        ClearCapturedLog();
        muduo::Logger::setOutput(&CaptureLogOutput);

        loopThread_ = std::make_unique<muduo::net::EventLoopThread>();
        loop_ = loopThread_->startLoop();
        port_ = FindFreeLoopbackPort();
        ASSERT_NE(port_, 0);

        RunInLoopAndWait([this] {
            server_ = std::make_unique<muduo::net::TcpServer>(
                loop_, muduo::net::InetAddress(port_, /*loopbackOnly=*/true), "fake_redis");
            // 回调只捕获夹具 this:server_ 在 TearDown 里先于夹具销毁,属于 AGENTS.md §11.7 的同寿命例外。
            // 服务端不保存任何 TcpConnectionPtr,按连接名索引自己的记录。
            server_->setConnectionCallback([this](const muduo::net::TcpConnectionPtr& conn) {
                OnServerConnection(conn);
            });
            server_->setMessageCallback(
                [this](const muduo::net::TcpConnectionPtr& conn, muduo::net::Buffer* buf, muduo::Timestamp) {
                    OnServerMessage(conn, buf);
                });
            server_->start();   // 在 loop 线程上即时 listen,随后的 connect 不会撞拒连
        });
    }

    void TearDown() override
    {
        if (loop_ != nullptr)
        {
            RunInLoopAndWait([this] {
                if (manager_)
                {
                    manager_->Shutdown();
                    manager_.reset();
                }
                server_.reset();
            });
            // 再转一圈:让 removeChannel 排队的 dummy(持有 Channel)等 functor 在 loop 停下前跑完,
            // 而不是留给 ~EventLoop 随 pendingFunctors_ 一起丢弃(同 hiredis_command_lifecycle_test)。
            RunInLoopAndWait([] {});
        }
        loopThread_.reset();   // quit + join:此后不再有线程写日志
        loop_ = nullptr;
        muduo::Logger::setOutput(&StdoutLogOutput);
        muduo::Logger::setLogLevel(savedLogLevel_);
    }

    template <typename F>
    void RunInLoopAndWait(F&& fn)
    {
        std::promise<void> done;
        std::future<void> finished = done.get_future();
        loop_->runInLoop([&] { fn(); done.set_value(); });
        if (finished.wait_for(kSettleTimeout) != std::future_status::ready)
        {
            // 不能直接 return:loop 线程上的闭包按引用捕获了本栈帧,提前返回会让它访问已失效的栈。
            // loop 线程卡死时测试也无从恢复,记一条失败后立即终止进程,而不是无限期挂住。
            ADD_FAILURE() << "the loop thread did not run the task within the deadline; aborting";
            std::fflush(stdout);
            std::abort();
        }
    }

    // 在截止时间(单调时钟)前反复到 loop 线程上求值 pred,成立即返回 true。
    template <typename Pred>
    bool WaitUntil(Pred pred, std::chrono::milliseconds timeout)
    {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        for (;;)
        {
            bool satisfied = false;
            RunInLoopAndWait([&] { satisfied = pred(); });
            if (satisfied)
            {
                return true;
            }
            if (std::chrono::steady_clock::now() >= deadline)
            {
                return false;
            }
            std::this_thread::sleep_for(kPollInterval);
        }
    }

    // 在整个观察窗口内 pred 都成立才返回 true;用于断言「不该发生」的事。
    template <typename Pred>
    bool HoldsFor(Pred pred, std::chrono::milliseconds window)
    {
        const auto deadline = std::chrono::steady_clock::now() + window;
        for (;;)
        {
            bool holds = false;
            RunInLoopAndWait([&] { holds = pred(); });
            if (!holds)
            {
                return false;
            }
            if (std::chrono::steady_clock::now() >= deadline)
            {
                return true;
            }
            std::this_thread::sleep_for(kPollInterval);
        }
    }

    void ConfigureServer(AuthReplyMode mode, bool disconnectAfterFirstAuthOk)
    {
        RunInLoopAndWait([this, mode, disconnectAfterFirstAuthOk] {
            authMode_ = mode;
            disconnectAfterFirstAuthOk_ = disconnectAfterFirstAuthOk;
        });
    }

    // 在 loop 线程上建被测的 RedisManager 并发起首次连接。
    void StartManager(const std::string& password, double reconnectIntervalSec)
    {
        RunInLoopAndWait([this, password, reconnectIntervalSec] {
            manager_ = std::make_unique<RedisManager>();
            manager_->SetReconnectCallback([this] { OnManagerReconnected(); });
            RedisConnectOptions options;
            options.password = password;
            options.reconnectIntervalSec = reconnectIntervalSec;
            manager_->Connect(loop_, muduo::net::InetAddress("127.0.0.1", port_), std::move(options));
        });
    }

    Snapshot TakeSnapshot()
    {
        Snapshot snap;
        RunInLoopAndWait([this, &snap] {
            snap.connections = connections_;
            snap.serverDowns = serverDowns_;
            snap.malformedFrames = malformedFrames_;
            snap.reconnectCalls = reconnectCalls_;
            snap.pingReplies = pingReplies_;
            snap.pingNullReplies = pingNullReplies_;
            snap.pingSendFailures = pingSendFailures_;
            snap.zoneRedisPresent = ZoneRedisPresent();
            snap.zoneRedisConnected = ZoneRedisConnected();
        });
        return snap;
    }

    // ---- 以下只在 loop 线程上执行 ----

    bool ZoneRedisPresent() const
    {
        return manager_ && manager_->GetZoneRedis();
    }

    bool ZoneRedisConnected() const
    {
        return ZoneRedisPresent() && manager_->GetZoneRedis()->connected();
    }

    // 收到过至少一条命令的连接数:被截止定时器拆掉的半成品连接(建连都没完成)不计。
    std::size_t ConnectionsWithCommands() const
    {
        std::size_t count = 0;
        for (const FakeConnection& connection : connections_)
        {
            if (!connection.commands.empty())
            {
                ++count;
            }
        }
        return count;
    }

    // RedisManager 的重连回调。照生产用法(scene 在回调里补发积压),回调里立刻往新连接上发一条命令。
    void OnManagerReconnected()
    {
        ++reconnectCalls_;
        const RedisManager::HiredisPtr& redis = manager_->GetZoneRedis();
        if (!redis)
        {
            ++pingSendFailures_;
            return;
        }
        const int ret = redis->command(
            [this](hiredis::Hiredis*, redisReply* reply) {
                if (reply != nullptr)
                {
                    ++pingReplies_;
                }
                else
                {
                    ++pingNullReplies_;
                }
            },
            "PING");
        if (ret != REDIS_OK)
        {
            ++pingSendFailures_;
        }
    }

    void OnServerConnection(const muduo::net::TcpConnectionPtr& conn)
    {
        if (conn->connected())
        {
            connectionIndexByName_[conn->name()] = connections_.size();
            connections_.emplace_back();
            return;
        }
        ++serverDowns_;
    }

    void OnServerMessage(const muduo::net::TcpConnectionPtr& conn, muduo::net::Buffer* buf)
    {
        const auto it = connectionIndexByName_.find(conn->name());
        if (it == connectionIndexByName_.end())
        {
            ++malformedFrames_;
            buf->retrieveAll();
            return;
        }
        const std::size_t index = it->second;
        for (;;)
        {
            std::size_t consumed = 0;
            RespCommand cmd;
            const ParseResult result =
                ParseCommand(std::string_view(buf->peek(), buf->readableBytes()), consumed, cmd);
            if (result == ParseResult::kIncomplete)
            {
                return;
            }
            if (result == ParseResult::kMalformed)
            {
                ++malformedFrames_;
                buf->retrieveAll();
                return;
            }
            buf->retrieve(consumed);
            connections_[index].commands.push_back(cmd);
            ReplyTo(conn, index, cmd);
        }
    }

    void ReplyTo(const muduo::net::TcpConnectionPtr& conn, std::size_t index, const RespCommand& cmd)
    {
        FakeConnection& fake = connections_[index];
        if (fake.stalled)
        {
            return;
        }
        const std::string verb = VerbOf(cmd);
        if (verb == "AUTH")
        {
            switch (authMode_)
            {
            case AuthReplyMode::kOk:
                conn->send("+OK\r\n");
                if (disconnectAfterFirstAuthOk_ && !disconnectDone_)
                {
                    disconnectDone_ = true;
                    // 半关写端:+OK 发完后才发 FIN,客户端先处理应答、再读到 EOF 视为断线。
                    // 此后 send 一律被 muduo 丢弃(state 已是 kDisconnecting)。
                    conn->shutdown();
                }
                return;
            case AuthReplyMode::kWrongPass:
                conn->send("-WRONGPASS invalid username-password pair\r\n");
                return;
            case AuthReplyMode::kEchoPassword:
                conn->send("-ERR bad password " + (cmd.size() > 1 ? cmd.back() : std::string()) + "\r\n");
                return;
            case AuthReplyMode::kSwallow:
                fake.stalled = true;
                return;
            }
            return;
        }
        if (verb == "PING")
        {
            conn->send("+PONG\r\n");
            return;
        }
        conn->send("-ERR unknown command\r\n");
    }

    // loopThread_ 排第一:最后析构。TearDown 已显式 reset 它,这里只是兜底顺序。
    std::unique_ptr<muduo::net::EventLoopThread> loopThread_;
    muduo::net::EventLoop* loop_ = nullptr;
    uint16_t port_ = 0;
    muduo::Logger::LogLevel savedLogLevel_ = muduo::Logger::WARN;
    std::unique_ptr<muduo::net::TcpServer> server_;
    std::unique_ptr<RedisManager> manager_;

    // 以下只在 loop 线程上读写;测试线程经 RunInLoopAndWait 读取(latch 提供 happens-before)。
    AuthReplyMode authMode_ = AuthReplyMode::kOk;
    bool disconnectAfterFirstAuthOk_ = false;   // 只对第一次 AUTH 成功生效
    bool disconnectDone_ = false;
    std::map<std::string, std::size_t> connectionIndexByName_;   // TcpServer 生成的连接名在本 server 内唯一
    std::vector<FakeConnection> connections_;
    int serverDowns_ = 0;
    int malformedFrames_ = 0;
    int reconnectCalls_ = 0;      // RedisManager 重连回调被调用的次数
    int pingReplies_ = 0;         // 回调里发出的 PING 拿到非空应答的次数
    int pingNullReplies_ = 0;     // 回调里发出的 PING 以空应答释放的次数(连接断开或被释放)
    int pingSendFailures_ = 0;    // 回调里拿不到连接或 command() 返回非 OK 的次数
};

} // namespace

// ---------------------------------------------------------------------------
// 1. 空口令:不发 AUTH,连上即对业务可见,连接上的第一条命令就是重连回调自己发的;回调恰好一次。
// ---------------------------------------------------------------------------

TEST_F(RedisManagerAuth, EmptyPasswordSendsNoAuth)
{
    StartManager("", kLongReconnectIntervalSec);
    ASSERT_TRUE(WaitUntil([this] { return pingReplies_ >= 1; }, kSettleTimeout))
        << "the PING sent from the reconnect callback never got a reply";

    const Snapshot snap = TakeSnapshot();
    EXPECT_EQ(snap.reconnectCalls, 1);
    EXPECT_EQ(snap.pingSendFailures, 0);
    EXPECT_EQ(snap.malformedFrames, 0);
    EXPECT_TRUE(snap.zoneRedisConnected);
    ASSERT_EQ(snap.connections.size(), 1u);
    EXPECT_EQ(DescribeCommands(snap.connections[0].commands), "PING")
        << "without a password the first command must be the one sent by the reconnect callback";
    for (const FakeConnection& connection : snap.connections)
    {
        for (const RespCommand& cmd : connection.commands)
        {
            EXPECT_NE(VerbOf(cmd), "AUTH") << "AUTH must not be sent when the password is empty";
        }
    }
    EXPECT_FALSE(Contains(CapturedLog(), "Redis AUTH"));
}

// ---------------------------------------------------------------------------
// 2. 有口令:AUTH 是连接上的第一条命令;+OK 之后连接才对业务可见,重连回调里发的 PING 紧随其后;回调恰好一次。
// ---------------------------------------------------------------------------

TEST_F(RedisManagerAuth, AuthIsFirstCommandAndCallbackFollows)
{
    StartManager(kTestPassword, kLongReconnectIntervalSec);
    ASSERT_TRUE(WaitUntil([this] { return pingReplies_ >= 1; }, kSettleTimeout))
        << "the PING sent from the reconnect callback never got a reply";

    const Snapshot snap = TakeSnapshot();
    EXPECT_EQ(snap.reconnectCalls, 1);
    EXPECT_EQ(snap.pingSendFailures, 0);
    EXPECT_EQ(snap.malformedFrames, 0);
    EXPECT_TRUE(snap.zoneRedisConnected);
    ASSERT_EQ(snap.connections.size(), 1u);
    // <pw> 表示该参数与测试口令逐字节相等(见 DescribeCommand)。
    EXPECT_EQ(DescribeCommands(snap.connections[0].commands), "AUTH <pw> | PING");

    // 回调只在 +OK 之后才被调用,所以 PING 拿到应答时 OnAuthReply 的日志已经写入。
    const std::string log = CapturedLog();
    EXPECT_TRUE(Contains(log, "Redis AUTH ok to")) << MaskPassword(log);
    EXPECT_FALSE(Contains(log, kTestPassword)) << "password leaked into the log";
}

// ---------------------------------------------------------------------------
// 3. AUTH 被拒(fail-closed):连接不对业务可见、不调回调、握手连接上只有 AUTH;打 ERROR,日志不含口令;
//    在回调栈外重建握手连接重试,新连接的第一条命令仍是 AUTH。
// ---------------------------------------------------------------------------

TEST_F(RedisManagerAuth, RejectedAuthKeepsZoneRedisUnavailableAndRetries)
{
    ConfigureServer(AuthReplyMode::kWrongPass, /*disconnectAfterFirstAuthOk=*/false);
    StartManager(kTestPassword, kReconnectIntervalSec);
    // 至少两条连接收到过 AUTH = 被拒之后确实重建重试了。
    ASSERT_TRUE(WaitUntil(
        [this] { return ConnectionsWithCommands() >= 2 && Contains(CapturedLog(), "Redis AUTH rejected by"); },
        kSettleTimeout))
        << "a rejected AUTH was not retried on a new connection within the deadline";

    const Snapshot snap = TakeSnapshot();
    EXPECT_EQ(snap.reconnectCalls, 0) << "a connection whose AUTH was rejected must never be handed to the business";
    EXPECT_TRUE(snap.zoneRedisPresent) << "GetZoneRedis() must stay non-null after Connect";
    EXPECT_FALSE(snap.zoneRedisConnected) << "GetZoneRedis() must report not connected while AUTH keeps failing";
    EXPECT_EQ(snap.pingSendFailures, 0);
    EXPECT_EQ(snap.malformedFrames, 0);
    for (std::size_t i = 0; i < snap.connections.size(); ++i)
    {
        const std::vector<RespCommand>& cmds = snap.connections[i].commands;
        if (cmds.empty())
        {
            continue;   // 建连都没完成就被截止定时器拆掉的半成品连接
        }
        EXPECT_EQ(DescribeCommands(cmds), "AUTH <pw>")
            << "connection #" << i << ": nothing but AUTH may be sent before authentication succeeds";
    }

    const std::string log = CapturedLog();
    EXPECT_TRUE(Contains(log, "WRONGPASS invalid username-password pair"))
        << "server text without the password should be logged as is: " << MaskPassword(log);
    EXPECT_TRUE(Contains(log, "metric=zone_redis_auth_failed")) << MaskPassword(log);
    EXPECT_TRUE(Contains(log, "Attempting Redis reconnect")) << MaskPassword(log);
    EXPECT_FALSE(Contains(log, "Redis AUTH ok to"));
    EXPECT_FALSE(Contains(log, kTestPassword)) << "password leaked into the log";
}

// ---------------------------------------------------------------------------
// 4. 服务端把口令回显进错误文本:只保留错误码,其余换成 <redacted>,日志里没有口令原文。
// ---------------------------------------------------------------------------

TEST_F(RedisManagerAuth, PasswordEchoedByServerIsRedacted)
{
    ConfigureServer(AuthReplyMode::kEchoPassword, /*disconnectAfterFirstAuthOk=*/false);
    StartManager(kTestPassword, kLongReconnectIntervalSec);
    ASSERT_TRUE(WaitUntil([] { return Contains(CapturedLog(), "Redis AUTH rejected by"); }, kSettleTimeout))
        << "the rejected AUTH was never logged";

    const Snapshot snap = TakeSnapshot();
    EXPECT_EQ(snap.reconnectCalls, 0);
    ASSERT_EQ(snap.connections.size(), 1u);
    ASSERT_FALSE(snap.connections[0].commands.empty());
    EXPECT_EQ(DescribeCommand(snap.connections[0].commands.front()), "AUTH <pw>")
        << "precondition: the fake server echoes exactly the password it received";

    const std::string log = CapturedLog();
    EXPECT_TRUE(Contains(log, "ERR <redacted>"))
        << "the error code must survive, everything after it must be redacted: " << MaskPassword(log);
    EXPECT_FALSE(Contains(log, kTestPassword)) << "password echoed by the server leaked into the log";
    EXPECT_FALSE(Contains(log, "bad password"))
        << "the server text after the error code must be replaced, not just the password: " << MaskPassword(log);
}

// ---------------------------------------------------------------------------
// 5. AUTH 成功后服务端主动断开:限时内重连,新连接上的第一条命令仍是 AUTH;重连回调累计 2 次。
// ---------------------------------------------------------------------------

TEST_F(RedisManagerAuth, ReauthenticatesAfterServerDisconnect)
{
    ConfigureServer(AuthReplyMode::kOk, /*disconnectAfterFirstAuthOk=*/true);
    StartManager(kTestPassword, kReconnectIntervalSec);
    // 第 1 条连接上的 PING 撞上服务端半关,只会以空应答释放;所以非空的 PING 应答只可能来自重连后的连接。
    ASSERT_TRUE(WaitUntil([this] { return reconnectCalls_ >= 2 && pingReplies_ >= 1; }, kSettleTimeout))
        << "no successful reconnect within the deadline";

    const Snapshot snap = TakeSnapshot();
    EXPECT_EQ(snap.reconnectCalls, 2);
    EXPECT_EQ(snap.pingSendFailures, 0);
    EXPECT_EQ(snap.malformedFrames, 0);
    EXPECT_TRUE(snap.zoneRedisConnected);
    ASSERT_GE(snap.connections.size(), 2u);
    ASSERT_FALSE(snap.connections[0].commands.empty());
    EXPECT_EQ(DescribeCommand(snap.connections[0].commands.front()), "AUTH <pw>");

    // 之后每条发过命令的连接,第一条都是 AUTH;只有完成握手、移交给业务的那一条会出现回调发的 PING,且恰好一条。
    // (截止定时器若在某次建连或 AUTH 应答前到点,会拆掉那条半成品连接,服务端多看到一条没有命令或只有 AUTH 的连接。)
    std::size_t promoted = 0;
    for (std::size_t i = 1; i < snap.connections.size(); ++i)
    {
        const std::vector<RespCommand>& cmds = snap.connections[i].commands;
        if (cmds.empty())
        {
            continue;
        }
        EXPECT_EQ(DescribeCommand(cmds.front()), "AUTH <pw>") << "connection #" << i;
        for (const RespCommand& cmd : cmds)
        {
            if (VerbOf(cmd) == "PING")
            {
                ++promoted;
                break;
            }
        }
    }
    EXPECT_EQ(promoted, 1u);

    const std::string log = CapturedLog();
    EXPECT_TRUE(Contains(log, "Redis disconnected")) << MaskPassword(log);
    EXPECT_EQ(CountOccurrences(log, "Redis AUTH ok to"), 2u) << MaskPassword(log);
    EXPECT_FALSE(Contains(log, kTestPassword)) << "password leaked into the log";
}

// ---------------------------------------------------------------------------
// 6. 握手中(AUTH 被压住不回)Shutdown:握手期间连接不对业务可见;Shutdown 不崩、不回调、不重连,
//    服务端不再出现新连接。
// ---------------------------------------------------------------------------

TEST_F(RedisManagerAuth, ShutdownDuringHandshakeIsQuiet)
{
    ConfigureServer(AuthReplyMode::kSwallow, /*disconnectAfterFirstAuthOk=*/false);
    // 长间隔:截止定时器在整个用例期间不会到点,握手停在「AUTH 已发、没有应答」。
    StartManager(kTestPassword, kLongReconnectIntervalSec);
    ASSERT_TRUE(WaitUntil(
        [this] { return connections_.size() == 1 && !connections_[0].commands.empty(); },
        kSettleTimeout))
        << "AUTH never reached the fake server";

    bool presentDuringHandshake = false;
    bool connectedDuringHandshake = true;
    std::size_t connectionsAtShutdown = 0;
    RunInLoopAndWait([this, &presentDuringHandshake, &connectedDuringHandshake, &connectionsAtShutdown] {
        presentDuringHandshake = ZoneRedisPresent();
        connectedDuringHandshake = ZoneRedisConnected();
        manager_->Shutdown();
        connectionsAtShutdown = connections_.size();
    });
    EXPECT_TRUE(presentDuringHandshake) << "GetZoneRedis() must stay non-null while the handshake is in flight";
    EXPECT_FALSE(connectedDuringHandshake) << "the connection must not be usable before AUTH succeeds";

    const Snapshot snap = TakeSnapshot();
    ASSERT_EQ(snap.connections.size(), 1u);
    EXPECT_EQ(DescribeCommands(snap.connections[0].commands), "AUTH <pw>");
    EXPECT_EQ(snap.reconnectCalls, 0);
    EXPECT_EQ(snap.pingReplies, 0);
    EXPECT_EQ(snap.pingNullReplies, 0);
    EXPECT_FALSE(snap.zoneRedisPresent) << "Shutdown must release the zone Redis object";

    EXPECT_TRUE(WaitUntil([this] { return serverDowns_ >= 1; }, kSettleTimeout))
        << "Shutdown did not close the handshake connection";
    EXPECT_TRUE(HoldsFor(
        [this, connectionsAtShutdown] { return connections_.size() == connectionsAtShutdown && reconnectCalls_ == 0; },
        kQuietWindow))
        << "no new connection and no reconnect callback are allowed after Shutdown";

    const std::string log = CapturedLog();
    EXPECT_FALSE(Contains(log, "Redis AUTH ok to"));
    EXPECT_FALSE(Contains(log, "Redis AUTH rejected by"));
    EXPECT_FALSE(Contains(log, "Redis disconnected")) << "Shutdown must not go through the disconnect/reconnect path";
    EXPECT_FALSE(Contains(log, "Attempting Redis reconnect"));
    EXPECT_FALSE(Contains(log, kTestPassword)) << "password leaked into the log";
}

// ---------------------------------------------------------------------------
// 7. AUTH 一直没有应答:一个重连间隔后在回调栈外重建握手连接;全程不对业务可见、不调回调,
//    每条连接上只有一条 AUTH。
// ---------------------------------------------------------------------------

TEST_F(RedisManagerAuth, AuthTimeoutRebuildsConnection)
{
    ConfigureServer(AuthReplyMode::kSwallow, /*disconnectAfterFirstAuthOk=*/false);
    StartManager(kTestPassword, kReconnectIntervalSec);
    ASSERT_TRUE(WaitUntil([this] { return ConnectionsWithCommands() >= 2; }, kSettleTimeout))
        << "an unanswered AUTH did not lead to a rebuilt handshake connection within the deadline";

    const Snapshot snap = TakeSnapshot();
    EXPECT_EQ(snap.reconnectCalls, 0);
    EXPECT_TRUE(snap.zoneRedisPresent);
    EXPECT_FALSE(snap.zoneRedisConnected);
    EXPECT_EQ(snap.pingSendFailures, 0);
    EXPECT_EQ(snap.malformedFrames, 0);
    for (std::size_t i = 0; i < snap.connections.size(); ++i)
    {
        const std::vector<RespCommand>& cmds = snap.connections[i].commands;
        if (cmds.empty())
        {
            continue;
        }
        EXPECT_EQ(DescribeCommands(cmds), "AUTH <pw>") << "connection #" << i;
    }

    const std::string log = CapturedLog();
    EXPECT_TRUE(Contains(log, "Attempting Redis reconnect")) << MaskPassword(log);
    EXPECT_FALSE(Contains(log, "Redis AUTH ok to"));
    EXPECT_FALSE(Contains(log, "Redis AUTH rejected by"));
    EXPECT_FALSE(Contains(log, kTestPassword)) << "password leaked into the log";
}

// ---------------------------------------------------------------------------
// 8. 重连间隔非正数:打 WARN 并回落到默认值,连接照常建立。
// ---------------------------------------------------------------------------

TEST_F(RedisManagerAuth, NonPositiveReconnectIntervalFallsBackToDefault)
{
    StartManager("", 0.0);
    // WARN 在 Connect 里同步写出,StartManager 返回时已在截获的日志里。
    EXPECT_TRUE(Contains(CapturedLog(), "Redis reconnect interval 0s is not positive, using 3s")) << CapturedLog();
    ASSERT_TRUE(WaitUntil([this] { return pingReplies_ >= 1; }, kSettleTimeout))
        << "the connection did not come up with the fallback interval";

    const Snapshot snap = TakeSnapshot();
    EXPECT_EQ(snap.reconnectCalls, 1);
    EXPECT_TRUE(snap.zoneRedisConnected);
}
