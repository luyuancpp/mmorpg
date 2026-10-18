package dispatcher

// 订阅面的 miniredis 集成用例。
//
// 本文件刻意只用改造前就存在的入口(NewTaskResultDispatcher / Start / Register / Stop)和频道
// 字面值,不引用 go/shared/taskresult 等新符号:单独放到改造前的提交上也能编译,拿到断言层面的
// 红证据(改造前只订旧频道,实例频道一个都没有)。字面值同时钉住与已部署 go/db 之间的线上契约。
//
// 这里的等待只用来等异步订阅建立与回调到达,有上限,不拿时间判断对错。

import (
	"testing"
	"time"

	db_proto "proto/db"

	"github.com/alicebob/miniredis/v2"
	"github.com/redis/go-redis/v9"
	"google.golang.org/protobuf/proto"
)

const (
	wireLegacyNotifyChannel  = "task:result:notify"
	wireInstanceNotifyPrefix = "task:result:notify:"
	subscribeWaitLimit       = 5 * time.Second
)

type subscribeDelivery struct {
	res *db_proto.TaskResult
	err error
}

// waitForCondition 每 10ms 轮询一次 cond,超过 subscribeWaitLimit 仍不成立即失败。
func waitForCondition(t *testing.T, what string, cond func() bool) {
	t.Helper()
	deadline := time.Now().Add(subscribeWaitLimit)
	for !cond() {
		if time.Now().After(deadline) {
			t.Fatalf("timed out after %v waiting for %s", subscribeWaitLimit, what)
		}
		time.Sleep(10 * time.Millisecond)
	}
}

func TestStart_SubscribesLegacyAndInstanceChannels(t *testing.T) {
	mr := miniredis.RunT(t)
	rc := redis.NewClient(&redis.Options{Addr: mr.Addr()})
	t.Cleanup(func() { _ = rc.Close() })

	d := NewTaskResultDispatcher(rc, time.Minute)
	d.Start()
	defer d.Stop()

	// 旧频道必须仍然订着(混跑期旧版 go/db 只发它),同时恰好多出一条本实例频道。
	var instanceChannel string
	waitForCondition(t, "legacy and per-instance subscriptions", func() bool {
		channels := mr.PubSubChannels(wireInstanceNotifyPrefix + "*")
		if len(channels) != 1 {
			return false
		}
		instanceChannel = channels[0]
		n := mr.PubSubNumSub(wireLegacyNotifyChannel, instanceChannel)
		return n[wireLegacyNotifyChannel] == 1 && n[instanceChannel] == 1
	})

	for _, c := range []struct {
		name    string
		channel string
		taskID  string
	}{
		{"实例频道", instanceChannel, "t-instance"},
		{"旧频道", wireLegacyNotifyChannel, "t-legacy"},
	} {
		t.Run(c.name, func(t *testing.T) {
			got := make(chan subscribeDelivery, 1)
			d.Register(c.taskID, time.Minute, func(res *db_proto.TaskResult, err error) {
				got <- subscribeDelivery{res: res, err: err}
			})
			raw, err := proto.Marshal(&db_proto.TaskResult{Success: true, Error: c.name})
			if err != nil {
				t.Fatal(err)
			}
			// go/db 的顺序:先写结果 LIST,再发通知。
			if _, err := mr.Lpush("task:result:"+c.taskID, string(raw)); err != nil {
				t.Fatal(err)
			}
			if receivers := mr.Publish(c.channel, c.taskID); receivers != 1 {
				t.Fatalf("PUBLISH %s reached %d subscribers, want exactly 1", c.channel, receivers)
			}
			select {
			case r := <-got:
				if r.err != nil || !r.res.GetSuccess() || r.res.GetError() != c.name {
					t.Fatalf("callback got res=%v err=%v, want the pushed success result", r.res, r.err)
				}
			case <-time.After(subscribeWaitLimit):
				t.Fatalf("callback for %s not invoked within %v", c.taskID, subscribeWaitLimit)
			}
		})
	}
}
