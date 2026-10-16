#pragma once

#include "engine/core/type_define/type_define.h"
#include <muduo/contrib/hiredis/Hiredis.h>
#include <muduo/net/InetAddress.h>
#include <muduo/net/TimerId.h>

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>

// zone Redis 的建连参数。
struct RedisConnectOptions
{
	// 重连间隔的默认值;Connect 收到非正数时也回落到它。
	static constexpr double kDefaultReconnectIntervalSec = 3.0;
	// 存活探测的默认值;Connect 收到非正数时也回落到它们。
	static constexpr double kDefaultLivenessProbeIntervalSec = 5.0;
	static constexpr double kDefaultLivenessTimeoutSec = 15.0;

	// 空 = 不认证(dev / 本机无口令 Redis,行为与 2026-10 之前相同);
	// 非空 = 每条新连接(首次建连与每次重连)建立后发出的第一条命令就是 AUTH,收到 +OK 才对业务可见。
	std::string password;
	// 断线重连间隔(秒),同时是一次 AUTH 握手的截止时间(从连上算起)。
	// 可注入只是为了让回归用例不必等满默认值;生产保持默认。
	double reconnectIntervalSec = kDefaultReconnectIntervalSec;
	// 存活探测(契约见 RedisManager::Connect):每隔这么多秒检查一次对业务可见的连接,没有探测 PING 在途就发一条。
	double livenessProbeIntervalSec = kDefaultLivenessProbeIntervalSec;
	// 探测 PING 发出后这么多秒仍没有应答,就把连接当成已断开:摘下、释放、走既有重连。
	// 两项可注入同样只为回归用例;生产保持默认。
	double livenessTimeoutSec = kDefaultLivenessTimeoutSec;
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
	// 只有完成握手的连接(空口令 = 连上;非空口令 = AUTH 回 +OK)才会放到这里。握手中、AUTH 失败、断线待重连
	// (含存活探测判定无应答、已被摘下)期间,
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
	// - AUTH 被拒(口令错、服务端没开口令、服务端在 AUTH 前报错等)、发不出去,或从连上起一个重连间隔内没有应答
	//   (含应答前连接就被关闭,那时另有一条断线 ERROR):不移交,对业务等同于「Redis 不可用」;每个重连间隔在回调栈外
	//   重建握手连接再试一次。每次失败打一条带 metric=zone_redis_auth_failed 的 ERROR:被拒、发不出去当场打,
	//   没有应答在截止时打("Redis AUTH timed out on ...")。告警按这个标记抓。
	//   不能放行未认证的连接(fail-open):部分调用方把 ERROR 应答当业务结论处理,例如 battle 结算外发箱
	//   (BattleRoomManager::ProbeAndRetryOne)把非预期应答当成已销账并删条目,NOAUTH 会被误读成玩家资产结论。
	// - Hiredis::command() 在未连接时直接返回 REDIS_ERR,不会把命令缓存到连上之后,
	//   所以不存在「建连前排队的命令跑到 AUTH 前面」的情况。
	// - 口令只在 AUTH 命令里出现,任何日志都不打印它;服务端错误文本里含口令原文时遮掉。
	//
	// 存活契约(2026-10 起):针对 Redis 代理所在机器整机消失、客户端收不到 FIN / RST 的情况。没有它,这条连接会一直
	// 显示 connected(),命令只进不出,本进程的 Redis 读写全部卡住直到进程重启。TCP keepalive 不顶用:连接上有未确认的
	// 数据(繁忙进程或在途的 PING)时内核根本不发 keepalive 探测。
	// - 只探测已移交给业务、仍连着的连接。握手连接、占位对象、已断开的旧连接一律不发 PING:握手连接上 AUTH 仍是唯一的
	//   命令(上面的 fail-closed 契约不变);握手与断线期间分别由 AUTH 截止与重连定时器负责。
	// - 每 livenessProbeIntervalSec 秒检查一次:没有探测 PING 在途就发一条(同一时刻最多一条,不叠加)。
	//   这条 PING 的任何非空应答(+PONG,或 -BUSY 之类的错误应答)都算连接活着。
	// - PING 发出满 livenessTimeoutSec 秒仍没有应答:打一条带 metric=zone_redis_liveness_timeout 的 ERROR,把连接从
	//   GetZoneRedis() 摘下(换成占位对象)并释放,然后走既有重连(每个重连间隔重建握手连接,有口令照常先 AUTH)。
	//   释放时在途命令(含这条 PING)全部以空应答回调 —— 对业务的效果与 Redis 主切换时代理掐断连接完全相同,
	//   调用方的空应答路径必须本来就能承受断线。
	// - Redis 按序应答,PING 排在它之前发出的所有命令后面。所以「Redis 卡住超过截止」(长 Lua 脚本、大 KEYS、
	//   积压的命令一个截止内跑不完)同样会触发摘下重连:对本进程而言那段时间 Redis 等同于不可用,这是有意的。
	// - 检测时延:最后一次正常往返之后,最迟约 livenessTimeoutSec + livenessProbeIntervalSec(默认 20 秒)判定断线。
	// - 摘下与释放不在定时器回调里做,而是 queueInLoop 到本轮的 pending functor 阶段:定时器回调与 socket 事件
	//   同属一轮事件分发,在那里销毁一条本轮也有 I/O 的连接,Linux 是释放后使用、Windows Debug 撞 removeChannel 断言
	//   (见下方「已知残余风险」)。排队期间这条连接若已自行断开、或已被 Connect / 移交替换,就什么也不做。
	void Connect(muduo::net::EventLoop *loop, const muduo::net::InetAddress &addr, RedisConnectOptions options);

	// 显式取消重连与存活探测定时器,作废已排队的摘下任务,并释放 Hiredis Channel。必须在 EventLoop 析构前调用;
	// 幂等,Node 正常停机与入口函数兜底可以重复执行。
	void Shutdown();

	// 每次有连接完成握手、移交给 GetZoneRedis() 之后(首次建连与每次重连)在同一调用栈里同步调用。
	// 配了口令时此刻 AUTH 已经成功,回调里发的命令排在 AUTH 之后。
	// 回调运行在被移交连接自己的 hiredis 回调栈内(空口令时是连接回调,有口令时是 AUTH 的命令回调,都已置
	// REDIS_IN_CALLBACK):回调里不得同步调用 Connect() / Shutdown(),也不得释放或替换 GetZoneRedis() ——
	// 那会在 Hiredis 自己的回调栈里销毁它:redisAsyncFree 被推迟,随后的 cleanup 拿已析构的对象回调(释放后使用)。
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
	void StartLivenessTimer();
	void CancelLivenessTimer();
	void OnLivenessTick();
	void OnLivenessPingReply(std::uint64_t generation, const redisReply *reply);
	void QueueDropUnresponsive();
	void DropUnresponsive(std::uint64_t generation);
	void ResetLivenessProbe();

	// 存活探测定时器与排进 queueInLoop 的摘下任务用它判断本对象是否还活着、是否还处在同一个 Connect 周期。
	// 本类不由 shared_ptr 持有(tlsRedis 是 thread_local 值),拿不到 weak_ptr<自己>,用这枚令牌代替(AGENTS.md §11.7):
	// 闭包只捕获它的 weak_ptr 和 this,先 lock,失败即返回,成功才碰 this(单线程 loop,lock 与使用之间不会被析构)。
	// Connect 换新令牌、Shutdown(析构经 Shutdown)清空令牌,此前排队的闭包一律失效。
	struct LifetimeToken
	{
	};

	// 生命周期(AGENTS.md §11.7):两个 Hiredis 都由本对象独占持有,它们上面的连接 / 断线 / 命令回调(含存活探测的 PING)
	// 都捕获裸 this,属于「与被绑对象同寿命」的例外。释放前先清连接与断线回调;释放时挂起的 AUTH / PING 回调以空应答
	// 进来,OnAuthReply / OnLivenessPingReply 开头就返回,不碰任何成员。连接只在本类的成员函数里释放(含摘下任务),
	// 从不移交给生命周期不受本类控制的闭包,所以这些回调触发时本对象一定还活着。
	// 重连定时器(runEvery)同样捕获裸 this,但它归 EventLoop 的 TimerQueue 所有,不属于同寿命例外:只靠 Shutdown()
	// (析构也会调它)取消,所以 Shutdown 必须在 EventLoop 析构前执行 —— tlsRedis 是 thread_local,析构可能晚于
	// EventLoop,不能指望析构兜底。存活探测定时器与摘下任务另有 lifetimeToken_ 守护,Shutdown 照样取消定时器。
	// 不变量:重连定时器只在 zoneRedis_ 不可用时运行,所以移交时被替换掉的只会是占位对象或已断开的旧连接
	// (二者 context 都已为空,析构不触发任何回调)。存活探测摘下连接时先换上占位对象再 ScheduleReconnect,不破坏它。
	//
	// 已知残余风险(Linux 构建):DoReconnect 在定时器回调里同步销毁握手连接。Linux 的定时器走 timerfd,与 Hiredis 的
	// socket Channel 同属一批 activeChannels_;截止定时器排在前面时,握手连接的 Channel 随后仍会在同一批里被 handleEvent,
	// 而 Hiredis 的读写回调绑的是裸 this、没有 Channel::tie,结果是释放后使用(Debug 下先撞 EventLoop::removeChannel 的
	// 断言)。触发需要握手连接的 I/O 事件(建连完成、AUTH 应答、服务端断开)恰与截止落在同一批。改动前「建连中被重连定时器
	// 拆掉」就是同一类窗口,本次多了「AUTH 应答 / 被拒后断开恰在截止时刻」两种。Windows 先处理完 I/O 再跑定时器,
	// 没有释放后使用,只剩 Debug 下同一断言的误报。根治在 Hiredis 适配层(shared_ptr 持有 + Channel::tie,连改动前的
	// 建连窗口一并消除);不要在这里用裸 this 的 queueInLoop 推迟释放,那违反 §11.7。
	// 存活探测摘下的是对业务可见、本轮可能正有 I/O 的连接,窗口远比握手连接宽,所以它不在定时器回调里释放,而是经
	// lifetimeToken_ 守护的 queueInLoop 推到本轮 pending functor 阶段(此时 Channel 事件已全部分发完、eventHandling_ 为假),
	// 不受上述窗口影响。
	HiredisPtr zoneRedis_;      // 对业务可见:已完成握手的连接、已断开的旧连接或占位对象
	HiredisPtr handshakeRedis_; // 正在建连或等 AUTH 应答的连接;没有握手在进行时为空
	// 不拥有 EventLoop。用 reference_wrapper 而非裸 EventLoop*,满足 no-raw-pointer-member 检查
	// (与 RedisSystem / KafkaManager 同一写法);未 Connect 或已 Shutdown 时为空。
	std::optional<std::reference_wrapper<muduo::net::EventLoop>> loop_;
	muduo::net::InetAddress redisAddr_;
	RedisConnectOptions options_;
	muduo::net::TimerId reconnectTimerId_;
	bool reconnectTimerActive_ = false;
	// AUTH 已在当前握手连接上发出、还没有任何非空应答。SendAuth 成功时置位;收到非空应答、DoReconnect 报超时、
	// ResetConnections 时清零。DoReconnect 看到它仍为真,就说明截止前没有应答,要按 AUTH 超时打 ERROR。
	bool authAwaitingReply_ = false;
	ReconnectCallback reconnectCb_;

	// 存活探测。定时器从 Connect 跑到 Shutdown,每一跳自己判断当前有没有可探测的连接。
	muduo::net::TimerId livenessTimerId_;
	bool livenessTimerActive_ = false;
	// 探测代号:每次有连接移交给业务、被探测摘下或被显式拆除都加一。PING 回调与摘下任务捕获发出时的代号,
	// 对不上就说明它属于已被替换的连接,直接忽略。
	std::uint64_t livenessGeneration_ = 0;
	bool livenessPingOutstanding_ = false;                    // 当前连接上有一条探测 PING 还没有应答
	std::chrono::steady_clock::time_point livenessPingSentAt_; // 那条 PING 发出的时刻(单调时钟)
	std::shared_ptr<LifetimeToken> lifetimeToken_;           // 见 LifetimeToken;未 Connect 或已 Shutdown 时为空
};

extern thread_local RedisManager tlsRedis;
