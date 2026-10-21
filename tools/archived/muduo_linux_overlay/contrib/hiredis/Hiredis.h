#ifndef MUDUO_CONTRIB_HIREDIS_HIREDIS_H
#define MUDUO_CONTRIB_HIREDIS_HIREDIS_H

#include "muduo/base/noncopyable.h"
#include "muduo/base/StringPiece.h"
#include "muduo/base/Types.h"
#include "muduo/net/Callbacks.h"
#include "muduo/net/InetAddress.h"

#include <hiredis/hiredis.h>
#include <memory>

struct redisAsyncContext;

namespace muduo
{
namespace net
{
class Channel;
class EventLoop;
}
}

namespace hiredis
{

class Hiredis : public std::enable_shared_from_this<Hiredis>,
                muduo::noncopyable
{
 public:
  typedef std::function<void(Hiredis*, int)> ConnectCallback;
  typedef std::function<void(Hiredis*, int)> DisconnectCallback;
  typedef std::function<void(Hiredis*, redisReply*)> CommandCallback;

  // 只能由工厂创建。所有操作和最后一个强引用的释放必须在 loop 线程，且 loop 必须仍然存活。
  // I/O 分发期间释放最后一个引用时，整个对象、context 和 fd 统一延至 pending 阶段销毁。
  // 空 loop 只用于从不 connect 的不可用占位对象。
  static std::shared_ptr<Hiredis> Create(muduo::net::EventLoop* loop,
                                       const muduo::net::InetAddress& serverAddr);
  ~Hiredis();

  const muduo::net::InetAddress& serverAddress() const { return serverAddr_; }
  // redisAsyncContext* context() { return context_; }
  bool connected() const;
  const char* errstr() const;

  void setConnectCallback(const ConnectCallback& cb) { connectCb_ = cb; }
  void setDisconnectCallback(const DisconnectCallback& cb) { disconnectCb_ = cb; }

  void connect();
  void disconnect();

  int command(const CommandCallback& cb, muduo::StringArg cmd, ...);

  int ping();

 private:
  Hiredis(muduo::net::EventLoop* loop, const muduo::net::InetAddress& serverAddr);
  void handleRead(muduo::Timestamp receiveTime);
  void handleWrite();

  int fd() const;
  void logConnection(bool up) const;
  void setChannel();
  void removeChannel();

  void connectCallback(int status);
  void disconnectCallback(int status);

  static Hiredis* getHiredis(const redisAsyncContext* ac);

  static void connectCallback(const redisAsyncContext* ac, int status);
  static void disconnectCallback(const redisAsyncContext* ac, int status);
  // command callback
  static void commandCallback(redisAsyncContext* ac, void*, void*);

  static void addRead(void* privdata);
  static void delRead(void* privdata);
  static void addWrite(void* privdata);
  static void delWrite(void* privdata);
  static void cleanup(void* privdata);

 private:
  muduo::net::EventLoop* loop_;
  const muduo::net::InetAddress serverAddr_;
  redisAsyncContext* context_;
  std::shared_ptr<muduo::net::Channel> channel_;
  ConnectCallback connectCb_;
  DisconnectCallback disconnectCb_;
};

}  // namespace hiredis

#endif  // MUDUO_CONTRIB_HIREDIS_HIREDIS_H
