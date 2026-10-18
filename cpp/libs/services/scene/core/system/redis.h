#pragma once

#include <functional>
#include <memory>
#include <optional>

#include "engine/infra/storage/redis_client/redis_client.h"
#include "proto/common/database/player_cache.pb.h"
#include <muduo/net/TimerId.h>

using PlayerDataRedis = std::unique_ptr<MessageAsyncClient<Guid, PlayerAllData>>;

namespace muduo
{
    namespace net
    {
        class EventLoop;
    }
}

class RedisSystem
{
public:
    RedisSystem() = default;
    ~RedisSystem();

    RedisSystem(const RedisSystem&) = delete;
    RedisSystem& operator=(const RedisSystem&) = delete;
    RedisSystem(RedisSystem &&) = delete;
    RedisSystem &operator=(RedisSystem &&) = delete;

    PlayerDataRedis& GetPlayerDataRedis() {
		return playerRedis;
	}

    void Initialize(muduo::net::EventLoop *loop);

    // 进入停机 drain:只停止会制造新存盘的周期任务,保留 Redis 重连与重试定时器,
    // 让已经发起的玩家退出存盘仍有机会收到 ACK。
    void BeginShutdown();

    // 停 DBTask 回执 Pump(50ms 定时器)并收账(DbTaskDispatchSystem::Shutdown:注销回执监听器、有未落定玩家时打一行
    // 带玩家号的 ERROR)。幂等;Initialize 没跑过或已 Shutdown 时是空操作。
    // 由 scene main.cpp 挂在 Node::SetBeforeTeardown 上:那一刻 producer 的最后一次 flush 已计入账本、日志还在写文件。
    // Node 析构之后(Shutdown 里的那次)再打的日志在 Linux 上会静默丢失,Pump 也会在半拆的节点上推下线收尾。
    void StopDbTaskDispatch();

    // 取消全部定时器并释放 playerRedis。幂等;必须在 EventLoop 析构前调用。
    void Shutdown();

private:
	PlayerDataRedis playerRedis;
    // Reference-wrapper instead of raw EventLoop* to satisfy the project's
    // no-raw-pointer-member lint (cpp/plugin). Set by Initialize(), unset means
    // Initialize() never ran, so Shutdown() is a no-op.
    std::optional<std::reference_wrapper<muduo::net::EventLoop>> loop_;
    muduo::net::TimerId retryTimerId_;
    muduo::net::TimerId snapshotTimerId_;
    muduo::net::TimerId periodicSaveTimerId_;
    // DBTask 投递回执的 50ms Pump(DbTaskDispatchSystem::Pump)。BeginShutdown 不取消它:停机 drain 期间
    // 下线收尾还要靠它把回执推完;StopDbTaskDispatch(Node 拆除前钩子)里取消,Shutdown 兜底。
    muduo::net::TimerId dbTaskPumpTimerId_;
    bool retryTimerActive_ = false;
    bool snapshotTimerActive_ = false;
    bool periodicSaveTimerActive_ = false;
    bool dbTaskPumpTimerActive_ = false;
    bool shutdownBegun_ = false;
};


extern thread_local RedisSystem tlsRedisSystem;
