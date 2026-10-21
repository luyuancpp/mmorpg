#include "contrib/hiredis/Hiredis.h"

#include "muduo/base/Logging.h"
#include "muduo/net/Channel.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/SocketsOps.h"

#include <hiredis/async.h>

using namespace muduo;
using namespace muduo::net;
using namespace hiredis;

static void dummy(const std::shared_ptr<Channel>&)
{
}

std::shared_ptr<Hiredis> Hiredis::Create(EventLoop* loop, const InetAddress& serverAddr)
{
  if (loop) loop->assertInLoopThread();
  return std::shared_ptr<Hiredis>(new Hiredis(loop, serverAddr), [loop](Hiredis* client)
  {
    if (loop) loop->assertInLoopThread();
    if (loop && loop->eventHandling())
    {
      // 此处裸指针是删除器接管的待销毁对象，不是回调借用的业务 this。
      // 必须保留整个对象和 socket：只延迟 Channel 注销会让 fd 被新连接复用、撞上旧 Poller 注册。
      loop->queueInLoop([client] { delete client; });
    }
    else
    {
      delete client;
    }
  });
}

Hiredis::Hiredis(EventLoop* loop, const InetAddress& serverAddr)
  : loop_(loop),
    serverAddr_(serverAddr),
    context_(NULL)
{
}

Hiredis::~Hiredis()
{
  LOG_DEBUG << this;
  // redisAsyncFree 先执行挂起 command 的 null-reply 回调,再通过 ev.cleanup
  // 注销 Channel。断言必须放在 free 之后,否则正常停机时活跃连接必然触发断言。
  if (context_)
  {
    ::redisAsyncFree(context_);
    context_ = NULL;
  }
  assert(!channel_ || channel_->isNoneEvent());
}

bool Hiredis::connected() const
{
  return channel_ && context_ && (context_->c.flags & REDIS_CONNECTED);
}

const char* Hiredis::errstr() const
{
  assert(context_ != NULL);
  return context_->errstr;
}

void Hiredis::connect()
{
  assert(loop_);
  loop_->assertInLoopThread();
  assert(!context_);

  context_ = ::redisAsyncConnect(serverAddr_.toIp().c_str(), serverAddr_.port());

  context_->ev.addRead = addRead;
  context_->ev.delRead = delRead;
  context_->ev.addWrite = addWrite;
  context_->ev.delWrite = delWrite;
  context_->ev.cleanup = cleanup;
  context_->ev.data = this;

  setChannel();

  assert(context_->onConnect == NULL);
  assert(context_->onDisconnect == NULL);
  ::redisAsyncSetConnectCallback(context_, connectCallback);
  ::redisAsyncSetDisconnectCallback(context_, disconnectCallback);
}

void Hiredis::disconnect()
{
  if (connected())
  {
    loop_->assertInLoopThread();
    // 无在途命令时 disconnect 可同步触发业务回调；即使调用方在其中 reset，也须等 C 调用完全返回。
    // 析构中的 null-reply 回调可能再请求断线，此时强引用已归零、context 已在 FREEING，直接忽略。
    const auto self = weak_from_this().lock();
    if (!self) return;
    LOG_DEBUG << this;
    ::redisAsyncDisconnect(context_);
  }
}

int Hiredis::fd() const
{
  assert(context_);
  return context_->c.fd;
}

void Hiredis::setChannel()
{
  LOG_DEBUG << this;
  assert(!channel_);
  channel_.reset(new Channel(loop_, fd()));
  channel_->tie(shared_from_this());
  const std::weak_ptr<Hiredis> weakSelf = shared_from_this();
  channel_->setReadCallback([weakSelf](Timestamp time)
  {
    if (const auto self = weakSelf.lock()) self->handleRead(time);
  });
  channel_->setWriteCallback([weakSelf]
  {
    if (const auto self = weakSelf.lock()) self->handleWrite();
  });
  // Windows 的纯 HUP 不走读回调,仍须让 hiredis 读取 EOF、释放挂起命令并通知断线。
  // tie 保住一次完整事件分发；weak 回调让最后一个外部引用被释放后的同批事件失效。
  channel_->setCloseCallback([weakSelf]
  {
    if (const auto self = weakSelf.lock()) self->handleRead(Timestamp::now());
  });
}

void Hiredis::removeChannel()
{
  LOG_DEBUG << this;
  // cleanup 与 disconnect callback 都可能走到这里;hiredis 明确允许 event
  // cleanup 重复执行,adapter 也必须幂等。
  if (!channel_)
  {
    return;
  }
  channel_->disableAll();
  channel_->remove();
  loop_->queueInLoop(std::bind(dummy, channel_));
  channel_.reset();
}

void Hiredis::handleRead(muduo::Timestamp receiveTime)
{
  // 同批 HUP / READ / WRITE 事件可能在上一回调里已释放 context,后续事件直接失效。
  if (!context_) return;
  LOG_TRACE << "receiveTime = " << receiveTime.toString();
  // HUP 也可能是建连阶段的首个事件;与写路径一致,让连接回调重新建立 Channel。
  if (!(context_->c.flags & REDIS_CONNECTED))
  {
    removeChannel();
  }
  ::redisAsyncHandleRead(context_);
}

void Hiredis::handleWrite()
{
  // Linux 的 HUP 分支之后仍会派发同批 OUT;EOF 回调可能已清空 context。
  if (!context_) return;
  if (!(context_->c.flags & REDIS_CONNECTED))
  {
    removeChannel();
  }
  ::redisAsyncHandleWrite(context_);
}

/* static */ Hiredis* Hiredis::getHiredis(const redisAsyncContext* ac)
{
  Hiredis* hiredis = static_cast<Hiredis*>(ac->ev.data);
  assert(hiredis->context_ == ac);
  return hiredis;
}

void Hiredis::logConnection(bool up) const
{
  InetAddress localAddr(sockets::getLocalAddr(fd()));
  InetAddress peerAddr(sockets::getPeerAddr(fd()));

  LOG_INFO << localAddr.toIpPort() << " -> "
           << peerAddr.toIpPort() << " is "
           << (up ? "UP" : "DOWN");
}

/* static */ void Hiredis::connectCallback(const redisAsyncContext* ac, int status)
{
  LOG_TRACE;
  getHiredis(ac)->connectCallback(status);
}

void Hiredis::connectCallback(int status)
{
  if (status != REDIS_OK)
  {
    LOG_ERROR << context_->errstr << " failed to connect to " << serverAddr_.toIpPort();
  }
  else
  {
    logConnection(true);
    setChannel();
  }

  // 业务回调可能 Shutdown / Connect 并清掉成员回调，局部副本保住正在执行的 target。
  const ConnectCallback callback = connectCb_;
  if (callback)
  {
    callback(this, status);
  }

  // 连接失败时 hiredis 会在 connect callback 返回后自动释放 context。
  // 外部 callback 仍可在返回前读取 errstr,随后必须清空 wrapper 中的悬空指针。
  if (status != REDIS_OK)
  {
    context_ = NULL;
  }
}

/* static */ void Hiredis::disconnectCallback(const redisAsyncContext* ac, int status)
{
  LOG_TRACE;
  getHiredis(ac)->disconnectCallback(status);
}

void Hiredis::disconnectCallback(int status)
{
  logConnection(false);
  removeChannel();

  const DisconnectCallback callback = disconnectCb_;
  if (callback)
  {
    callback(this, status);
  }

  // hiredis 在 disconnect callback 返回后立即释放 redisAsyncContext;
  // 及时清空,避免下次重连析构 wrapper 时对悬空指针二次 free。
  context_ = NULL;
}

void Hiredis::addRead(void* privdata)
{
  LOG_TRACE;
  Hiredis* hiredis = static_cast<Hiredis*>(privdata);
  hiredis->channel_->enableReading();
}

void Hiredis::delRead(void* privdata)
{
  LOG_TRACE;
  Hiredis* hiredis = static_cast<Hiredis*>(privdata);
  hiredis->channel_->disableReading();
}

void Hiredis::addWrite(void* privdata)
{
  LOG_TRACE;
  Hiredis* hiredis = static_cast<Hiredis*>(privdata);
  hiredis->channel_->enableWriting();
}

void Hiredis::delWrite(void* privdata)
{
  LOG_TRACE;
  Hiredis* hiredis = static_cast<Hiredis*>(privdata);
  hiredis->channel_->disableWriting();
}

void Hiredis::cleanup(void* privdata)
{
  Hiredis* hiredis = static_cast<Hiredis*>(privdata);
  LOG_DEBUG << hiredis;
  hiredis->removeChannel();
}

int Hiredis::command(const CommandCallback& cb, muduo::StringArg cmd, ...)
{
  if (!connected()) return REDIS_ERR;
  loop_->assertInLoopThread();

  LOG_TRACE;
  CommandCallback* p = new CommandCallback(cb);
  va_list args;
  va_start(args, cmd);
  int ret = ::redisvAsyncCommand(context_, commandCallback, p, cmd.c_str(), args);
  va_end(args);
  if (ret != REDIS_OK)
  {
    // hiredis 1.2 只在返回 REDIS_OK 的路径上登记回调(deps/hiredis/async.c __redisAsyncCommand):
    // DISCONNECTING / FREEING 拒收、命令格式化失败、登记回调时 OOM,都在登记之前就返回,此后不会再拿
    // 这个 privdata 回调,释放责任回到这里,不删就泄漏。典型触发:~Hiredis 里 redisAsyncFree 先用空应答
    // 回调挂起命令、后注销 Channel,那段时间 connected() 仍为真,回调里再发的命令会撞 FREEING。
    // 唯一例外是多频道 (P)SUBSCRIBE 中途 OOM(已写进订阅字典的频道仍持有 p),但本封装不支持订阅
    // (commandCallback 每次回调后都 delete)。升级 hiredis 时必须重核这条前提。
    // Windows 与 Linux 覆盖件必须保持同样的释放语义；端口访问器按各平台的 muduo 接口适配。
    delete p;
  }
  return ret;
}

/* static */ void Hiredis::commandCallback(redisAsyncContext* ac, void* r, void* privdata)
{
  redisReply* reply = static_cast<redisReply*>(r);
  std::unique_ptr<CommandCallback> cb(static_cast<CommandCallback*>(privdata));
  // 正常分发由 Channel tie 保活，析构中的 null-reply 由析构体同步 free 保活。
  // 回调返回后只清理本地回调副本，不再通过业务指针访问 wrapper。
  (*cb)(getHiredis(ac), reply);
}

int Hiredis::ping()
{
  return command([](Hiredis*, redisReply* reply)
  {
    if (reply && reply->str) LOG_DEBUG << reply->str;
  }, "PING");
}
