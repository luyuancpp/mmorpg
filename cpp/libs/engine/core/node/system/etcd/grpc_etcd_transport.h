#pragma once

#include <string>

#include "node/system/etcd/etcd_transport.h"

// 官方生成客户端 + CompletionQueue 的 etcd 传输(回退路径)。行为与改前逐项一致:
//   * 应答经生成层的全局 handler 回来(只在 status.ok() 时回调;失败什么都不做 —— 恢复靠 txn / lease 看门狗与 2s 全量 Range);
//   * Watch / LeaseKeepAlive 流断开后**不重建**,这正是 §10.2 P0-D 列出的缺陷,回退时依然存在;
//   * CQ 由 EtcdService 的 5ms 轮询驱动(那个定时器同时驱动进程里全部官方 gRPC 客户端,不归本类)。
class GrpcEtcdTransport final : public EtcdTransport
{
public:
    GrpcEtcdTransport(const std::string &endpoint, EtcdResponseSink &sink);
    ~GrpcEtcdTransport() override;
    GrpcEtcdTransport(const GrpcEtcdTransport &) = delete;
    GrpcEtcdTransport &operator=(const GrpcEtcdTransport &) = delete;

    const char *Name() const override { return "grpc"; }
    bool RebuildsStreams() const override { return false; }

    void Range(const etcdserverpb::RangeRequest &request) override;
    void Put(const etcdserverpb::PutRequest &request) override;
    void DeleteRange(const etcdserverpb::DeleteRangeRequest &request) override;
    void Txn(const etcdserverpb::TxnRequest &request) override;
    void LeaseGrant(const etcdserverpb::LeaseGrantRequest &request) override;
    void LeaseRevoke(const etcdserverpb::LeaseRevokeRequest &request) override;
    void WatchWrite(const etcdserverpb::WatchRequest &request) override;
    void KeepAliveWrite(const etcdserverpb::LeaseKeepAliveRequest &request) override;
    void Shutdown() override;

private:
    void InstallHandlers();

    // 生成层的全局 handler 捕获 this;Shutdown / 析构时一并清空,本对象没了回调自动失效(AGENTS.md §11.7)。
    EtcdResponseSink &sink_;
};
