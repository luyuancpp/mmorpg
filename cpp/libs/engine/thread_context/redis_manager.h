#pragma once

#include "engine/core/type_define/type_define.h"
#include <muduo/contrib/hiredis/Hiredis.h>
#include <muduo/net/InetAddress.h>
#include <muduo/net/TimerId.h>

#include <functional>
#include <optional>
#include <string>

// zone Redis 的建连参数。
struct RedisConnectOptions
{
	// 重连间隔的默认值;Connect 收到非正数时也回落到它。
	static constexpr double kDefaultReconnectIntervalSec = 3.0;

	// 空 = 不认证(dev / 本机无口令 Redis,行为与 2026-10 之前相同);
	// 非空 = 每条新连接(首次建连与每次重连)建立后发出的第一条命令就是 AUTH,收到 +OK 才对业务可见。
	std::string password;
	// 断线重连间隔(秒),同时是一次 AUTH 握手的截止时间(从连上算起)。
	// 可注入只是为了让回归用例不必等满默认值;生产保持默认。
	double reconnectIntervalSec = kDefaultReconnectIntervalSec;
};

class RedisManager
{
public:
	using HiredisPtr = std::unique_ptr<hiredis::Hiredis>;
	using ReconnectCallback = std::function<void()>;

	RedisManager() = default;
	~RedisManager();

	RedisManager(const RedisManager &) = delete;
	RedisManager &operator=(const RedisManager &) = delete;
	RedisManager(RedisManager &&) = delete;
	RedisManager &operator=(RedisManager &&) = delete;

	// 业务使用的 zone Redis 连接。Connect 之后、Shutdown 之前恒非空(Connect 之前与 Shutdown 之后为空,同改动前)。
	// 只有完成握手的连接(空口令 = 连上;非空口令 = AUTH 回 +OK)才会放到这里。握手中、AUTH 失败、断线待重连期间,
	// 这里是一个 connected()==false 的对象 —— 从不建连的占位对象,或已断开的旧连接 —— 它的 command() 直接返回
	// REDIS_ERR、不调回调,所以调用点既有的 `redis && redis->connected()` 判断与 REDIS_ERR 分支照常生效,不需要改。
	HiredisPtr &GetZoneRedis()
	{
		return zoneRedis_;
	}

	// 建立 zone Redis 连接并在断线后自动重连。必须在 loop 线程上调用;可重复调用(先拆掉已有连接)。
	// options 刻意不给默认值:调用方必须显式说明这条连接要不要认证。
	//
	// 认证契约(2026-10 起,docs/handoff/global-singletons-p0-findings-20261014.md P0-A,fail-closed):
	// - 每条新连接(首次建连与每次重连)先放在私有的握手位上,对业务不可见。password 非空时,
	//   连上后发出的第一条命令就是 AUTH,握手连接上不会出现任何其它命令。
	// - 空口令连上即移交;非空口令只在 AUTH 回 +OK 后移交给 GetZoneRedis(),并在同一调用栈里同步调用重连回调。
	//   所以重连回调补发的命令是这条连接上 AUTH 之后的第一批业务命令;scene 的 1s 重试定时器与各直发点在移交前
	//   只看得到 connected()==false,抢不到它们前面。
	// - AUTH 被拒(口令错、服务端没开口令、服务端在 AUTH 前报错等)、发不出去,或从连上起一个重连间隔内没有应答:
	//   不移交,对业务等同于「Redis 不可用」;每个重连间隔在回调栈外重建握手连接再试一次,每次失败打一条 ERROR。
	//   不能放行未认证的连接(fail-open):部分调用方把 ERROR 应答当业务结论处理,例如 battle 结算外发箱
	//   (BattleRoomManager::ProbeAndRetryOne)把非预期应答当成已销账并删条目,NOAUTH 会被误读成玩家资产结论。
	// - Hiredis::command() 在未连接时直接返回 REDIS_ERR,不会把命令缓存到连上之后,
	//   所以不存在「建连前排队的命令跑到 AUTH 前面」的情况。
	// - 口令只在 AUTH 命令里出现,任何日志都不打印它;服务端错误文本里含口令原文时遮掉。
	void Connect(muduo::net::EventLoop *loop, const muduo::net::InetAddress &addr, RedisConnectOptions options);

	// 显式取消重连并释放 Hiredis Channel。必须在 EventLoop 析构前调用;
	// 幂等,Node 正常停机与入口函数兜底可以重复执行。
	void Shutdown();

	// 每次有连接完成握手、移交给 GetZoneRedis() 之后(首次建连与每次重连)在同一调用栈里同步调用。
	// 配了口令时此刻 AUTH 已经成功,回调里发的命令排在 AUTH 之后。
	void SetReconnectCallback(const ReconnectCallback &cb) { reconnectCb_ = cb; }

private:
	void ScheduleReconnect();
	void CancelReconnectTimer();
	void DoReconnect();
	void StartHandshake();
	void OnHandshakeConnected(int status);
	void SendAuth();
	void OnAuthReply(hiredis::Hiredis *me, const redisReply *reply);
	void PromoteHandshake();
	void ResetConnections();

	// 生命周期(AGENTS.md §11.7):两个 Hiredis 都由本对象独占持有,它们上面的连接 / 断线 / 命令回调都捕获裸 this,
	// 属于「与被绑对象同寿命」的例外。释放前先清连接与断线回调;释放时挂起的 AUTH 回调以空应答进来,
	// OnAuthReply 开头就返回,不碰任何成员。
	// 不变量:重连定时器只在 zoneRedis_ 不可用时运行,所以移交时被替换掉的只会是占位对象或已断开的旧连接
	// (二者 context 都已为空,析构不触发任何回调)。
	HiredisPtr zoneRedis_;      // 对业务可见:已完成握手的连接、已断开的旧连接或占位对象
	HiredisPtr handshakeRedis_; // 正在建连或等 AUTH 应答的连接;没有握手在进行时为空
	// 不拥有 EventLoop。用 reference_wrapper 而非裸 EventLoop*,满足 no-raw-pointer-member 检查
	// (与 RedisSystem / KafkaManager 同一写法);未 Connect 或已 Shutdown 时为空。
	std::optional<std::reference_wrapper<muduo::net::EventLoop>> loop_;
	muduo::net::InetAddress redisAddr_;
	RedisConnectOptions options_;
	muduo::net::TimerId reconnectTimerId_;
	bool reconnectTimerActive_ = false;
	ReconnectCallback reconnectCb_;
};

extern thread_local RedisManager tlsRedis;
