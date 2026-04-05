#pragma once

#include "engine/core/type_define/type_define.h"
#include <muduo/contrib/hiredis/Hiredis.h>
#include <muduo/net/InetAddress.h>
#include <muduo/net/TimerId.h>

class RedisManager
{
public:
	using HiredisPtr = std::unique_ptr<hiredis::Hiredis>;

	HiredisPtr &GetZoneRedis()
	{
		return zoneRedis_;
	}

	// Call after creating and connecting the Hiredis instance.
	// Sets up a disconnect callback that triggers periodic reconnection.
	void SetupReconnect(muduo::net::EventLoop *loop, const muduo::net::InetAddress &addr);

private:
	void ScheduleReconnect();
	void CancelReconnectTimer();
	void DoReconnect();

	HiredisPtr zoneRedis_;
	muduo::net::EventLoop *loop_ = nullptr;
	muduo::net::InetAddress redisAddr_;
	muduo::net::TimerId reconnectTimerId_;
	bool reconnectTimerActive_ = false;
};

extern thread_local RedisManager tlsRedis;
