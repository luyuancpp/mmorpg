#include "redis_manager.h"

#include "muduo/base/Logging.h"
#include "muduo/net/EventLoop.h"

thread_local RedisManager tlsRedis;

void RedisManager::SetupReconnect(muduo::net::EventLoop *loop, const muduo::net::InetAddress &addr)
{
	loop_ = loop;
	redisAddr_ = addr;

	if (!zoneRedis_)
	{
		return;
	}

	zoneRedis_->setDisconnectCallback([this](hiredis::Hiredis *, int status)
									  {
		LOG_ERROR << "Redis disconnected (status=" << status << "), scheduling reconnect to "
				  << redisAddr_.toIpPort();
		ScheduleReconnect(); });
}

void RedisManager::ScheduleReconnect()
{
	static constexpr double kReconnectIntervalSec = 3.0;
	CancelReconnectTimer();
	reconnectTimerId_ = loop_->runEvery(kReconnectIntervalSec, [this]
										{ DoReconnect(); });
	reconnectTimerActive_ = true;
}

void RedisManager::CancelReconnectTimer()
{
	if (reconnectTimerActive_)
	{
		loop_->cancel(reconnectTimerId_);
		reconnectTimerActive_ = false;
	}
}

void RedisManager::DoReconnect()
{
	LOG_INFO << "Attempting Redis reconnect to " << redisAddr_.toIpPort();

	// Destroy the old (disconnected) Hiredis instance
	zoneRedis_.reset();

	// Create a new Hiredis and connect
	zoneRedis_ = std::make_unique<HiredisPtr::element_type>(loop_, redisAddr_);

	zoneRedis_->setConnectCallback([this](hiredis::Hiredis *, int status)
								   {
		if (status == REDIS_OK)
		{
			LOG_INFO << "Redis reconnected successfully to " << redisAddr_.toIpPort();
			CancelReconnectTimer();
		}
		else
		{
			LOG_WARN << "Redis reconnect attempt failed (status=" << status << "), will retry";
		} });

	zoneRedis_->setDisconnectCallback([this](hiredis::Hiredis *, int status)
									  {
		LOG_ERROR << "Redis disconnected again (status=" << status << "), scheduling reconnect";
		ScheduleReconnect(); });

	zoneRedis_->connect();
}
