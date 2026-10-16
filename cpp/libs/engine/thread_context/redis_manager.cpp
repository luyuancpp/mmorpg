#include "redis_manager.h"

#include "muduo/base/Logging.h"
#include "muduo/net/EventLoop.h"

#include <string_view>

namespace
{
	// AUTH 失败时服务端错误文本最多打印这么多字节。
	constexpr std::size_t kMaxAuthErrorLogBytes = 200;

	// 保持原先"传空 loop 就等于未绑定"的语义,不对空指针解引用。
	std::optional<std::reference_wrapper<muduo::net::EventLoop>> ToLoopRef(muduo::net::EventLoop *loop)
	{
		if (loop == nullptr)
		{
			return std::nullopt;
		}
		return std::ref(*loop);
	}

	// AUTH 失败时给日志用的描述。口令绝不进日志:服务端错误文本里若含口令原文(某些代理会回显),
	// 只保留第一个空格前的错误码(WRONGPASS / ERR / NOAUTH ...)供运维区分原因,其余换成 <redacted>;
	// 错误码本身也含口令(口令很短或恰是错误码的一部分)时整段换掉。
	// 必须先在完整文本上查口令、再截断:先截断会把跨截断点的口令前半段放进日志。
	std::string DescribeAuthFailure(const redisReply *reply, const std::string &password)
	{
		if (reply->type != REDIS_REPLY_ERROR || reply->str == nullptr)
		{
			return "unexpected reply type " + std::to_string(reply->type);
		}
		const std::string_view text(reply->str, reply->len);
		if (password.empty() || text.find(password) == std::string_view::npos)
		{
			return std::string(text.substr(0, kMaxAuthErrorLogBytes));
		}
		const std::string_view code = text.substr(0, text.find(' '));
		if (code.empty() || code.size() > kMaxAuthErrorLogBytes || code.find(password) != std::string_view::npos)
		{
			return "<redacted>";
		}
		return std::string(code) + " <redacted>";
	}

	// 先清连接与断线回调再释放:避免显式拆除又走一遍断线 / 重连路径。
	// 命令回调保留:redisAsyncFree 用空应答回调挂起命令,上层据此把写入放回重试队列。
	void ReleaseConnection(RedisManager::HiredisPtr &connection)
	{
		if (!connection)
		{
			return;
		}
		connection->setConnectCallback({});
		connection->setDisconnectCallback({});
		connection.reset();
	}
}

thread_local RedisManager tlsRedis;

RedisManager::~RedisManager()
{
	Shutdown();
}

void RedisManager::Connect(muduo::net::EventLoop *loop, const muduo::net::InetAddress &addr, RedisConnectOptions options)
{
	if (loop_)
	{
		CancelReconnectTimer();
	}
	ResetConnections();
	loop_ = ToLoopRef(loop);
	redisAddr_ = addr;
	options_ = std::move(options);
	if (!(options_.reconnectIntervalSec > 0))
	{
		LOG_WARN << "Redis reconnect interval " << options_.reconnectIntervalSec << "s is not positive, using "
				 << RedisConnectOptions::kDefaultReconnectIntervalSec << "s";
		options_.reconnectIntervalSec = RedisConnectOptions::kDefaultReconnectIntervalSec;
	}

	// 占位对象:只构造、从不 connect()。它的 connected() 恒为 false、command() 直接返回 REDIS_ERR 且不调回调,
	// context 为空,析构什么也不做(Hiredis.cc 的 connected() / command() / ~Hiredis)。有了它,GetZoneRedis()
	// 在第一次握手完成前就非空,直接 `GetZoneRedis()->command(` 的调用点不会解引用空指针。
	zoneRedis_ = std::make_unique<HiredisPtr::element_type>(loop, redisAddr_);
	StartHandshake();
}

void RedisManager::Shutdown()
{
	if (loop_)
	{
		CancelReconnectTimer();
	}

	// 先切断 RedisManager 自身的回调,否则 redisAsyncFree 触发 disconnect
	// callback 时会在停机过程中重新挂一条重连定时器。
	reconnectCb_ = {};
	ResetConnections();
	loop_.reset();
}

void RedisManager::StartHandshake()
{
	if (!loop_)
	{
		return;
	}

	// 回调在 connect() 之前装好:首个 SYN 就失败也能进 OnHandshakeConnected 挂上重连定时器。
	handshakeRedis_ = std::make_unique<HiredisPtr::element_type>(&loop_->get(), redisAddr_);
	handshakeRedis_->setConnectCallback([this](hiredis::Hiredis *, int status)
										{ OnHandshakeConnected(status); });
	// 断线回调装在连接对象上,移交后继续有效:握手期间断开与移交后断开都走这里。
	handshakeRedis_->setDisconnectCallback([this](hiredis::Hiredis *, int status)
										   {
		LOG_ERROR << "Redis disconnected (status=" << status << "), scheduling reconnect to "
				  << redisAddr_.toIpPort();
		ScheduleReconnect(); });
	handshakeRedis_->connect();
}

void RedisManager::OnHandshakeConnected(int status)
{
	if (status != REDIS_OK)
	{
		LOG_WARN << "Redis connect attempt failed (status=" << status
				 << ") to " << redisAddr_.toIpPort() << ", will retry";
		ScheduleReconnect();
		return;
	}

	LOG_INFO << "Redis connected to " << redisAddr_.toIpPort();
	CancelReconnectTimer();
	if (options_.password.empty())
	{
		// 不认证:连上即移交,重连回调与改动前同一时机。
		PromoteHandshake();
		return;
	}

	// 握手截止:从连上起一个重连间隔内没有 +OK,就由 DoReconnect 在回调栈外重建握手连接。
	// AUTH 被拒、发不出去时也靠这条定时器重试。
	ScheduleReconnect();
	SendAuth();
}

void RedisManager::SendAuth()
{
	// 握手连接上的第一条也是唯一一条命令。%b 按长度传参:口令里的空格、引号或二进制字节都不会被拆成多个参数。
	const int ret = handshakeRedis_->command(
		[this](hiredis::Hiredis *me, redisReply *reply)
		{ OnAuthReply(me, reply); },
		"AUTH %b", options_.password.data(), options_.password.size());
	if (ret == REDIS_OK)
	{
		authAwaitingReply_ = true;
		return;
	}
	// 连接回调里这条连接必然可用(hiredis 回调前已置 REDIS_CONNECTED,不会处在 DISCONNECTING / FREEING),
	// 所以只在命令格式化失败或内存不足时发生。不移交,截止定时器到点重建。
	LOG_ERROR << "Redis AUTH could not be sent to " << redisAddr_.toIpPort() << " (ret=" << ret
			  << ") -- zone Redis stays unavailable, retrying every " << options_.reconnectIntervalSec
			  << "s (metric=zone_redis_auth_failed)";
}

void RedisManager::OnAuthReply(hiredis::Hiredis *me, const redisReply *reply)
{
	// 空应答:这条连接正在释放或已断开,后续由断线回调 / 重连定时器负责;不移交,也不清 authAwaitingReply_ ——
	// 应答前被服务端断开的握手由截止时的 DoReconnect 按 AUTH 超时报告。
	// me 不是当前握手连接:被替换掉的旧握手连接的迟到回调。两种情况都不处理。
	if (reply == nullptr || me != handshakeRedis_.get())
	{
		return;
	}
	authAwaitingReply_ = false; // 有了应答,这次握手的结局在下面当场报告,截止时不再按超时报
	if (reply->type == REDIS_REPLY_STATUS)
	{
		LOG_INFO << "Redis AUTH ok to " << redisAddr_.toIpPort();
		PromoteHandshake();
		return;
	}

	// 失败一律不移交(fail-closed,见头文件契约)。多数情况下服务端会以 NOAUTH 拒绝这条连接上的后续命令,
	// 但服务端没开口令时命令会照常执行,所以不能把这条连接交给业务。
	// 不在这里销毁连接:回调栈是 Hiredis::commandCallback -> (*cb)(this, reply),在栈内销毁 Hiredis 会让
	// cleanup 钩子拿着已析构的对象调 removeChannel。截止定时器已在跑,到点由 DoReconnect 在回调栈外重建。
	LOG_ERROR << "Redis AUTH rejected by " << redisAddr_.toIpPort() << ": " << DescribeAuthFailure(reply, options_.password)
			  << " -- zone Redis stays unavailable, retrying every " << options_.reconnectIntervalSec
			  << "s (metric=zone_redis_auth_failed)";
	ScheduleReconnect(); // 截止定时器已在跑时是空操作;兜底保证一定会重试
}

void RedisManager::PromoteHandshake()
{
	CancelReconnectTimer();
	// unique_ptr 移动赋值:先让 zoneRedis_ 指向握手连接,再析构被替换的对象。被替换的只会是占位对象或已断开的旧连接
	// (见头文件不变量),析构不触发任何回调。握手连接本身没有被销毁,所以在它自己的连接 / 命令回调栈里移交是安全的。
	zoneRedis_ = std::move(handshakeRedis_);
	if (reconnectCb_)
	{
		reconnectCb_();
	}
}

void RedisManager::ScheduleReconnect()
{
	if (!loop_ || reconnectTimerActive_)
	{
		return; // Timer already running; avoid stacking duplicate timers.
	}
	reconnectTimerId_ = loop_->get().runEvery(options_.reconnectIntervalSec, [this]
											{ DoReconnect(); });
	reconnectTimerActive_ = true;
}

void RedisManager::CancelReconnectTimer()
{
	if (loop_ && reconnectTimerActive_)
	{
		loop_->get().cancel(reconnectTimerId_);
		reconnectTimerActive_ = false;
	}
}

void RedisManager::DoReconnect()
{
	if (authAwaitingReply_)
	{
		// AUTH 已发出,截止前没有任何应答:服务端卡住、中间代理只接受连接不转发,或应答前连接就被关了
		// (后者此前已有一条 "Redis disconnected" ERROR)。与被拒同样是握手失败、zone Redis 同样不可用,
		// 必须带 metric 打 ERROR,不能只剩下面那条 INFO(按 metric 标记的告警会漏掉)。
		LOG_ERROR << "Redis AUTH timed out on " << redisAddr_.toIpPort() << ": no reply within "
				  << options_.reconnectIntervalSec << "s of connecting -- zone Redis stays unavailable, retrying every "
				  << options_.reconnectIntervalSec << "s (metric=zone_redis_auth_failed)";
		authAwaitingReply_ = false;
	}
	LOG_INFO << "Attempting Redis reconnect to " << redisAddr_.toIpPort();

	// 只重建握手连接:建连失败、握手超时或 AUTH 被拒的旧握手连接在这里(回调栈外)销毁,
	// 挂起的 AUTH 以空应答回调,OnAuthReply 开头就返回。zoneRedis_ 里已断开的旧连接(或占位对象)保持不动,
	// 业务继续看到 connected()==false,直到新连接完成握手、移交时才被替换。每条新连接都会重新 AUTH。
	ReleaseConnection(handshakeRedis_);
	StartHandshake();
}

void RedisManager::ResetConnections()
{
	// 先拆握手连接(它的挂起 AUTH 以空应答回调,OnAuthReply 直接返回),再拆对业务可见的连接
	// (挂起的业务命令以空应答回调,同改动前)。显式拆除不是握手失败,不报 AUTH 超时。
	authAwaitingReply_ = false;
	ReleaseConnection(handshakeRedis_);
	ReleaseConnection(zoneRedis_);
}
