package inbox

import (
	"context"
	"errors"
	"fmt"
	"strconv"
	"testing"

	"player_inbox/internal/metrics"

	event "proto/common/event"
	kafkapb "proto/contracts/kafka"
	smpb "proto/scene_manager"

	"shared/kafkacmd"
	"shared/scenenode"

	"github.com/segmentio/kafka-go"
	"google.golang.org/protobuf/proto"
)

type fakeLocations struct {
	raw []byte
	err error
}

func (f fakeLocations) GetLocation(_ context.Context, _ string) ([]byte, error) { return f.raw, f.err }

type fakeNodes struct {
	synced bool
	entry  scenenode.NodeEntry
	err    error
}

func (f fakeNodes) Synced() bool { return f.synced }
func (f fakeNodes) EntryOf(_ uint32, _ string) (scenenode.NodeEntry, error) {
	return f.entry, f.err
}

type fakeWriter struct {
	msgs []kafka.Message
	err  error
}

func (f *fakeWriter) WriteMessages(_ context.Context, msgs ...kafka.Message) error {
	if f.err != nil {
		return f.err
	}
	f.msgs = append(f.msgs, msgs...)
	return nil
}

func locationBytes(t *testing.T, loc *smpb.PlayerLocation) []byte {
	t.Helper()
	b, err := proto.Marshal(loc)
	if err != nil {
		t.Fatal(err)
	}
	return b
}

const notifyEventID = 49

// TestKafkaNotifierOutcomes 钉住提醒的每种结局与「是否可以清 notify_due_ms」(设计 §6.3)。
// 评审 L6:不可恢复的两种(位置记录既无节点也无 epoch;镜像已同步却查不到节点)落定,交给落地拉取;
// 镜像未同步、节点身份歧义会自愈,留给补发(退避重试)。
func TestKafkaNotifierOutcomes(t *testing.T) {
	online := locationBytes(t, &smpb.PlayerLocation{NodeId: "7", ZoneId: 2, OwnerEpoch: 3})
	node := scenenode.NodeEntry{NodeId: 7, ZoneId: 2, NodeUuid: "uuid-7", Endpoint: "10.0.0.7:7000"}
	ambiguous := fmt.Errorf("%w zone=2 node=7: 2 条注册", scenenode.ErrNodeAmbiguous)
	cases := []struct {
		name    string
		loc     fakeLocations
		nodes   fakeNodes
		werr    error
		want    NotifyResult
		settled bool
		writes  int
	}{
		{"没有 location:离线视为成功", fakeLocations{}, fakeNodes{synced: true}, nil, NotifyOffline, true, 0},
		{"读 location 失败", fakeLocations{err: errors.New("redis down")}, fakeNodes{synced: true}, nil, NotifyError, false, 0},
		{"location 写坏", fakeLocations{raw: []byte{0xff}}, fakeNodes{synced: true}, nil, NotifyError, false, 0},
		{"等待跨 zone 落点:视为成功", fakeLocations{raw: locationBytes(t, &smpb.PlayerLocation{ZoneId: 2, OwnerEpoch: 4})},
			fakeNodes{synced: true}, nil, NotifyAwaitingPlacement, true, 0},
		{"无节点也无 epoch:不可恢复,落定交给落地拉取", fakeLocations{raw: locationBytes(t, &smpb.PlayerLocation{ZoneId: 2})},
			fakeNodes{synced: true}, nil, NotifyNoNode, true, 0},
		{"镜像未同步:留给补发", fakeLocations{raw: online}, fakeNodes{synced: false}, nil, NotifyNodesUnsynced, false, 0},
		{"镜像已同步却查不到节点:不可恢复,落定交给落地拉取", fakeLocations{raw: online},
			fakeNodes{synced: true, err: errors.New("节点未注册 zone=2 node=7")}, nil, NotifyNoNode, true, 0},
		{"节点身份歧义:租约过期后自愈,留给补发", fakeLocations{raw: online}, fakeNodes{synced: true, err: ambiguous},
			nil, NotifyNodeAmbiguous, false, 0},
		{"uuid 为空:不发(防僵尸)", fakeLocations{raw: online}, fakeNodes{synced: true, entry: scenenode.NodeEntry{NodeId: 7, ZoneId: 2}},
			nil, NotifyEmptyUUID, false, 0},
		{"写 Kafka 失败", fakeLocations{raw: online}, fakeNodes{synced: true, entry: node}, errors.New("broker down"), NotifyError, false, 0},
		{"送达", fakeLocations{raw: online}, fakeNodes{synced: true, entry: node}, nil, NotifySent, true, 1},
	}
	for _, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			w := &fakeWriter{err: tc.werr}
			n := &KafkaNotifier{Locations: tc.loc, Nodes: tc.nodes, Writer: w, EventID: notifyEventID}
			got := n.Notify(context.Background(), 42, gmDomain)
			if got != tc.want || got.Settled() != tc.settled || len(w.msgs) != tc.writes {
				t.Fatalf("Notify = %s (settled=%v, writes=%d), want %s (settled=%v, writes=%d)",
					got, got.Settled(), len(w.msgs), tc.want, tc.settled, tc.writes)
			}
		})
	}
}

// TestKafkaNotifierMessageContract 钉住写出的 SceneCommand:topic / 分区由 kafkacmd 寻址,key = player_id,
// target_instance_id = 节点 uuid,event_id 与载荷 = PlayerInboxNotifyEvent(AGENTS §7 #2 #3)。
func TestKafkaNotifierMessageContract(t *testing.T) {
	const playerID = uint64(1234567)
	w := &fakeWriter{}
	n := &KafkaNotifier{
		Locations: fakeLocations{raw: locationBytes(t, &smpb.PlayerLocation{NodeId: "7", ZoneId: 2, OwnerEpoch: 3})},
		Nodes:     fakeNodes{synced: true, entry: scenenode.NodeEntry{NodeId: 7, ZoneId: 2, NodeUuid: "uuid-7"}},
		Writer:    w,
		EventID:   notifyEventID,
	}
	if got := n.Notify(context.Background(), playerID, gmDomain); got != NotifySent {
		t.Fatalf("Notify = %s, want sent", got)
	}
	msg := w.msgs[0]
	want, err := kafkacmd.SceneCommandMessage("7", strconv.FormatUint(playerID, 10), nil)
	if err != nil {
		t.Fatal(err)
	}
	if msg.Topic != want.Topic || msg.Partition != want.Partition || string(msg.Key) != string(want.Key) {
		t.Fatalf("寻址 = %s/%d key=%s, want %s/%d key=%s", msg.Topic, msg.Partition, msg.Key, want.Topic, want.Partition, want.Key)
	}
	cmd := &kafkapb.SceneCommand{}
	if err := proto.Unmarshal(msg.Value, cmd); err != nil {
		t.Fatal(err)
	}
	if cmd.GetCommandType() != kafkapb.SceneCommand_DispatchEvent || cmd.GetPlayerId() != playerID ||
		cmd.GetTargetSceneId() != 7 || cmd.GetTargetInstanceId() != "uuid-7" || cmd.GetEventId() != notifyEventID {
		t.Fatalf("SceneCommand = %+v", cmd)
	}
	ev := &event.PlayerInboxNotifyEvent{}
	if err := proto.Unmarshal(cmd.GetPayload(), ev); err != nil {
		t.Fatal(err)
	}
	if ev.GetPlayerId() != playerID || ev.GetDomain() != uint32(gmDomain) {
		t.Fatalf("PlayerInboxNotifyEvent = %+v", ev)
	}
}

// TestKafkaNotifierIncompleteIsError:装配漏了任何一项都回 error,不 panic、不清 notify_due_ms。
func TestKafkaNotifierIncompleteIsError(t *testing.T) {
	var nilNotifier *KafkaNotifier
	if got := nilNotifier.Notify(context.Background(), 1, gmDomain); got != NotifyError {
		t.Fatalf("nil notifier = %s", got)
	}
	if got := (&KafkaNotifier{}).Notify(context.Background(), 1, gmDomain); got != NotifyError || got.Settled() {
		t.Fatalf("空 notifier = %s", got)
	}
}

// TestNotifyResultLabelsArePreregistered:每种结局的 notify_total{result} 标签互不相同,且与 metrics 启动时预建的
// 零值序列逐一对应 —— 新增结局忘了预建,告警的 increase(...) 会漏掉它第一次出现(metrics.Register 的注释)。
func TestNotifyResultLabelsArePreregistered(t *testing.T) {
	all := []NotifyResult{NotifyError, NotifySent, NotifyOffline, NotifyAwaitingPlacement, NotifyNoNode,
		NotifyEmptyUUID, NotifyNodesUnsynced, NotifyNodeAmbiguous}
	pre := map[string]bool{}
	for _, l := range metrics.NotifyResultLabels() {
		pre[l] = true
	}
	seen := map[string]bool{}
	for _, r := range all {
		label := r.String()
		if seen[label] {
			t.Errorf("标签 %q 被两种结局共用", label)
		}
		seen[label] = true
		if !pre[label] {
			t.Errorf("结局 %d 的标签 %q 没有在 metrics.NotifyResultLabels 里预建", r, label)
		}
	}
	if len(pre) != len(seen) {
		t.Errorf("预建标签 %v 与结局标签 %v 数目不一致(有多余或遗漏)", pre, seen)
	}
}
