package svc

import (
	"testing"
	"time"

	"login/internal/config"
	"login/internal/logic/pkg/loginqueue"
)

// 本文件盯住 G1b 的接线:出队 leader 锁按本 login 的 Node.ZoneId 拆,
// 锁键 = Queue.DispatcherLockKey + ":zone:" + ZoneId;开了排队却给不出合法 zone 时起服即失败。
//
// 这几个用例改写包级 config.AppConfig,所以本包用例都不能加 t.Parallel()。

func loginQueueTestConf(lockKeyBase string) config.QueueConf {
	return config.QueueConf{
		Enabled:           true,
		DispatchInterval:  30 * time.Millisecond,
		AdmitTTL:          60 * time.Second,
		QueueEntryTTL:     time.Hour,
		SoftCapMultiplier: 1.5,
		DispatcherLockTTL: 2 * time.Second,
		DispatcherLockKey: lockKeyBase,
	}
}

// GateWatcher 留 nil:队列为空时 dispatcher 不碰 gate 目录(见 loginqueue 的
// TestDispatchOnce_EmptyQueueSkipsGateDirectory),这一轮选主 + 空跑不会走到 CandidatesForZone。
// 注意本用例测不出「先判空再算容量」写反:写反时 nil watcher 的 panic 会被 safego 接住,
// 锁在 panic 之前已拿到,用例照样通过;顺序由上面那条 loginqueue 用例负责。
func TestInitLoginQueue_DispatcherLocksOwnZone(t *testing.T) {
	previous := config.AppConfig
	t.Cleanup(func() { config.AppConfig = previous })
	rdb, mr := newSvcTestRedis(t)

	const base = "dispatcher:lock:login_queue"
	config.AppConfig = config.Config{}
	config.AppConfig.Node.ZoneId = 7
	config.AppConfig.Queue = loginQueueTestConf(base)

	s := &ServiceContext{RedisClient: rdb}
	s.initLoginQueue()
	if s.QueueDispatcher == nil {
		t.Fatal("Queue.Enabled=true must build a dispatcher")
	}
	s.QueueDispatcher.Start()
	defer s.QueueDispatcher.Stop()

	want := loginqueue.ZoneDispatcherLockKey(base, 7)
	deadline := time.Now().Add(3 * time.Second)
	for !mr.Exists(want) {
		if time.Now().After(deadline) {
			t.Fatalf("zone 7 leader lock %q was never acquired", want)
		}
		time.Sleep(10 * time.Millisecond)
	}
	if mr.Exists(base) {
		t.Fatalf("legacy global lock key %q must not be written any more", base)
	}
}

func TestInitLoginQueue_ZeroZonePanicsWhenQueueEnabled(t *testing.T) {
	previous := config.AppConfig
	t.Cleanup(func() { config.AppConfig = previous })
	rdb, _ := newSvcTestRedis(t)

	config.AppConfig = config.Config{} // Node.ZoneId = 0
	config.AppConfig.Queue = loginQueueTestConf("dispatcher:lock:login_queue")

	defer func() {
		if recover() == nil {
			t.Fatal("Queue.Enabled=true with Node.ZoneId=0 must panic at startup (queue would only grow)")
		}
	}()
	(&ServiceContext{RedisClient: rdb}).initLoginQueue()
}

// 默认关:不构造队列和 dispatcher,也就不做任何新校验,与修复前一致。
func TestInitLoginQueue_DisabledSkipsDispatcherValidation(t *testing.T) {
	previous := config.AppConfig
	t.Cleanup(func() { config.AppConfig = previous })
	rdb, _ := newSvcTestRedis(t)

	config.AppConfig = config.Config{}
	config.AppConfig.Queue = loginQueueTestConf("")
	config.AppConfig.Queue.Enabled = false

	s := &ServiceContext{RedisClient: rdb}
	s.initLoginQueue()
	if s.LoginQueue != nil || s.QueueDispatcher != nil {
		t.Fatal("Queue.Enabled=false must build neither the queue nor the dispatcher")
	}
}
