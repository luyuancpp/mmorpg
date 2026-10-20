package kafka

// R7 复核 [0] 维持现实现:再平衡 / 交接让出 / 失败收尾(drainAfterStop)第一步 acker.stop,之后到达的异步入队确认不再
// 标记位点 —— 等确认的 offset 及其后已完成的 offset 一律由新属主重投。这里在 claimAcker 上把这条语义钉住(确定性,
// 不依赖 ConsumeClaim 的调度时序),规格与设计文档按它改写;要改成「先等确认再 stop」须另立设计,并同改这条用例。

import (
	"context"
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
)

func TestClaimAcker_StopDropsLateConfirmations(t *testing.T) {
	// 对照:不 stop 时,低位的确认一到,连续前缀一次推进到高位。
	ctrl := &recordingSession{ctx: context.Background()}
	a := newClaimAcker(ctrl, 0, func() {})
	a.track(offsetMsg(0)) // 等异步入队确认的那一条
	a.track(offsetMsg(1))
	a.ack(offsetMsg(1))
	assert.Empty(t, ctrl.marked(), "offset 1 cannot pass the unconfirmed offset 0")
	a.ack(offsetMsg(0))
	assert.Equal(t, []int64{0, 1}, ctrl.marked())

	// 再平衡收尾:stop 之后到达的确认(含等确认的那一条)不再标记;wait 照样等到它们(pending 归零)。
	session := &recordingSession{ctx: context.Background()}
	canceled := false
	b := newClaimAcker(session, 0, func() { canceled = true })
	b.track(offsetMsg(0))
	b.track(offsetMsg(1))
	b.ack(offsetMsg(1))
	b.stop()
	assert.True(t, canceled, "stop cancels the claim context")
	b.ack(offsetMsg(0))
	assert.Empty(t, session.marked(), "a confirmation arriving after acker.stop must not mark: offsets 0 and 1 replay on the new owner")
	assert.True(t, closedWithin(b.wait(), 5*time.Second), "pending is already zero: the drain waited for the late confirmation, it just does not mark")
}
