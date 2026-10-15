#pragma once

// etcd 访问的传输接缝(docs/design/grpc-muduo-transport.md §13.4)。
//
// EtcdHelper 组好请求后只经 EtcdTransport 发出;应答交给 EtcdResponseSink(EtcdService 实现)。
// 两个实现:
//   * GrpcEtcdTransport  —— 官方生成客户端 + CQ,行为与改前完全一致(回退路径,默认);
//   * MuduoEtcdTransport —— muduo_grpc 通道 + 双向流,流断按退避重建(§13.5)。只被各节点 main 的
//     InstallFromEnv() 引用:静态库按需取目标文件,不调它的工程(测试)不必链 muduo_grpc / nghttp2。
//
// 线程:全部接口只在节点 EventLoop 线程调用;sink 的回调也只在该线程、且永不在发起调用的栈内同步执行。

#include <functional>
#include <memory>
#include <string>

#include "proto/etcd/etcd.pb.h"

namespace muduo
{
namespace net
{
class EventLoop;
} // namespace net
} // namespace muduo

class EtcdResponseSink
{
public:
    virtual ~EtcdResponseSink() = default;

    // request:新传输带回原请求(对账要知道是哪个前缀);官方实现的成功回调拿不到请求,传 nullptr。
    virtual void OnRange(const etcdserverpb::RangeRequest *request, const etcdserverpb::RangeResponse &reply) = 0;
    virtual void OnPut(const etcdserverpb::PutResponse &reply) = 0;
    virtual void OnDeleteRange(const etcdserverpb::DeleteRangeResponse &reply) = 0;
    virtual void OnTxn(const etcdserverpb::TxnResponse &reply) = 0;
    virtual void OnLeaseGrant(const etcdserverpb::LeaseGrantResponse &reply) = 0;
    virtual void OnWatch(const etcdserverpb::WatchResponse &reply) = 0;
    virtual void OnKeepAlive(const etcdserverpb::LeaseKeepAliveResponse &reply) = 0;

    // 仅新传输:断掉的流已经换成新流(首条流不回调)。在新流上之前登记的 watch 全部失效,
    // 调用方要重新 Range → 对账 → 建 watch;KeepAlive 流要立即补一拍。
    virtual void OnWatchStreamRestarted() {}
    virtual void OnKeepAliveStreamRestarted() {}
};

class EtcdTransport
{
public:
    virtual ~EtcdTransport() = default;

    virtual const char *Name() const = 0;
    // true:传输自己负责双向流断开后的重建,并在重建后回调 OnWatchStreamRestarted / OnKeepAliveStreamRestarted;
    // EtcdService 据此走按前缀 Range → 对账 → watch 的新路径。false:沿用旧路径(watch canceled 后 2s 重发全部前缀)。
    virtual bool RebuildsStreams() const = 0;

    virtual void Range(const etcdserverpb::RangeRequest &request) = 0;
    virtual void Put(const etcdserverpb::PutRequest &request) = 0;
    virtual void DeleteRange(const etcdserverpb::DeleteRangeRequest &request) = 0;
    virtual void Txn(const etcdserverpb::TxnRequest &request) = 0;
    virtual void LeaseGrant(const etcdserverpb::LeaseGrantRequest &request) = 0;
    virtual void LeaseRevoke(const etcdserverpb::LeaseRevokeRequest &request) = 0;
    // 往 Watch / LeaseKeepAlive 双向流写一条请求。新传输:流断开、尚未重建时直接丢弃 ——
    // 重建后由 sink 的 Restarted 回调重发需要的请求,旧流上的请求没有意义。
    virtual void WatchWrite(const etcdserverpb::WatchRequest &request) = 0;
    virtual void KeepAliveWrite(const etcdserverpb::LeaseKeepAliveRequest &request) = 0;

    // 节点关机(FinalizeShutdownInLoop 阶段 0,loop 仍在运行):停止重建、释放连接;之后不再回调 sink。
    // 调用前已发出的 LeaseRevoke 尽量写出(§13.7),应答不等。
    virtual void Shutdown() = 0;
};

namespace etcd_transport
{
// endpoint 是 Etcd.Hosts 的第一项("host:port")。sink 的寿命必须覆盖返回的传输对象。
using Factory = std::function<std::unique_ptr<EtcdTransport>(muduo::net::EventLoop &loop, const std::string &endpoint,
                                                             EtcdResponseSink &sink)>;

// 进程级:必须在 Node 构造之前调用(EtcdService::Init 在 Node 构造函数里取工厂)。不设 = 官方实现。
void SetFactory(Factory factory);
// 按已装的工厂建传输;没装工厂(或工厂返回空)时建官方实现。
std::unique_ptr<EtcdTransport> Create(muduo::net::EventLoop &loop, const std::string &endpoint, EtcdResponseSink &sink);

// 读环境变量 NODE_ETCD_TRANSPORT:grpc(默认)| muduo_h2;不认识的值按 grpc 并打 WARN。
// 定义在 muduo_etcd_transport.cpp —— 只有调用它的节点可执行文件才链入新传输。
void InstallFromEnv();
} // namespace etcd_transport
