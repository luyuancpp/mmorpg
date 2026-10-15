package logic

// sync_digest_test.go —— 好友写操作对落地摘要的 bump(docs/design/landing-recovery-sync-digest.md §3.3 friend 三行)。
//
// 断言面是「给谁 bump 了哪些域」:用记录型假 Evaler 装进 playersync.Bumper,读它收到的 KEYS / ARGV。
// 键名与字段名测试自己拼(不调 playersync.Key):它们是 C++ scene 解析的跨语言契约,
// 生产端改了格式、测试跟着一起改,谁也发现不了。
// 最后一条用例在 miniredis 上经 go-zero 适配器真实执行 BumpScript,钉住装配形态(SharedRedis 句柄)。

import (
	"context"
	"errors"
	"sort"
	"strconv"
	"sync"
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"friend/internal/data"

	pb "proto/friend"
	scenepb "proto/scene"
	"shared/playersync"
)

// digestCall 是假 Evaler 收到的一次 bump:摘要键 + 域号十进制(ARGV[2..])。
type digestCall struct {
	key     string
	domains []string
}

type recordingDigest struct {
	mu    sync.Mutex
	calls []digestCall
	err   error
}

func (r *recordingDigest) Eval(_ context.Context, _ string, keys []string, args ...string) error {
	r.mu.Lock()
	defer r.mu.Unlock()
	call := digestCall{key: keys[0]}
	if len(args) > 1 {
		call.domains = append([]string(nil), args[1:]...)
	}
	r.calls = append(r.calls, call)
	return r.err
}

// bumped 把收到的 bump 整理成「摘要键 → 排好序的域号」。同一个键 bump 两次会被合并 ——
// 用例关心的是最终给谁动了哪些域,不是调用次数。
func (r *recordingDigest) bumped() map[string][]string {
	r.mu.Lock()
	defer r.mu.Unlock()
	out := map[string][]string{}
	for _, c := range r.calls {
		out[c.key] = append(out[c.key], c.domains...)
	}
	for k := range out {
		sort.Strings(out[k])
	}
	return out
}

// withSyncDigest 给夹具装上记录型摘要 bump。
func (f *fixture) withSyncDigest() *recordingDigest {
	ev := &recordingDigest{}
	f.svcCtx.SyncDigest = playersync.NewBumper(ev, "friend")
	return ev
}

// digestKey 拼 player:{id}:sync_digest。刻意不调 playersync.Key:见文件头。
func digestKey(playerID uint64) string {
	return "player:" + strconv.FormatUint(playerID, 10) + ":sync_digest"
}

// digestFields 把域转成排好序的域号十进制(hash 字段名)。
func digestFields(ds ...scenepb.SyncDomain) []string {
	out := make([]string, 0, len(ds))
	for _, d := range ds {
		out = append(out, strconv.Itoa(int(d)))
	}
	sort.Strings(out)
	return out
}

const (
	digestMe   uint64 = 6100
	digestPeer uint64 = 6101
)

var (
	domFriendList    = scenepb.SyncDomain_SYNC_DOMAIN_FRIEND_LIST
	domFriendRequest = scenepb.SyncDomain_SYNC_DOMAIN_FRIEND_REQUEST
	domFriendBlock   = scenepb.SyncDomain_SYNC_DOMAIN_FRIEND_BLOCK
)

// 成功路径:每个写操作 bump 的玩家与域都必须恰好是「拉取结果会变的人」(§3.5 第 3 条)。
func TestFriendWritesBumpSyncDigest(t *testing.T) {
	cases := []struct {
		name  string
		setup func(f *fixture)
		call  func(l *FriendLogic) (uint32, error)
		want  map[string][]string
	}{
		{
			name: "AddFriend:只有接收方的申请列表变",
			call: func(l *FriendLogic) (uint32, error) {
				r, err := l.AddFriend(playerCtx(digestMe), &pb.AddFriendRequest{TargetPlayerId: digestPeer})
				return r.GetErrorMessage().GetId(), err
			},
			want: map[string][]string{digestKey(digestPeer): digestFields(domFriendRequest)},
		},
		{
			name: "AcceptFriend:双方的好友列表与申请列表",
			call: func(l *FriendLogic) (uint32, error) {
				r, err := l.AcceptFriend(playerCtx(digestMe), &pb.AcceptFriendRequest{FromPlayerId: digestPeer})
				return r.GetErrorMessage().GetId(), err
			},
			want: map[string][]string{
				digestKey(digestMe):   digestFields(domFriendList, domFriendRequest),
				digestKey(digestPeer): digestFields(domFriendList, domFriendRequest),
			},
		},
		{
			name: "RejectFriend:只有拒绝方本人的申请列表",
			call: func(l *FriendLogic) (uint32, error) {
				r, err := l.RejectFriend(playerCtx(digestMe), &pb.RejectFriendRequest{FromPlayerId: digestPeer})
				return r.GetErrorMessage().GetId(), err
			},
			want: map[string][]string{digestKey(digestMe): digestFields(domFriendRequest)},
		},
		{
			name: "RemoveFriend:是好友时双方的好友列表",
			setup: func(f *fixture) {
				f.store.friends = []data.FriendEntry{{FriendPlayerID: digestPeer}}
			},
			call: func(l *FriendLogic) (uint32, error) {
				r, err := l.RemoveFriend(playerCtx(digestMe), &pb.RemoveFriendRequest{TargetPlayerId: digestPeer})
				return r.GetErrorMessage().GetId(), err
			},
			want: map[string][]string{
				digestKey(digestMe):   digestFields(domFriendList),
				digestKey(digestPeer): digestFields(domFriendList),
			},
		},
		{
			name: "Block:本人三个域,对方好友列表与申请列表",
			call: func(l *FriendLogic) (uint32, error) {
				r, err := l.Block(playerCtx(digestMe), &pb.BlockRequest{TargetPlayerId: digestPeer})
				return r.GetErrorMessage().GetId(), err
			},
			want: map[string][]string{
				digestKey(digestMe):   digestFields(domFriendBlock, domFriendList, domFriendRequest),
				digestKey(digestPeer): digestFields(domFriendList, domFriendRequest),
			},
		},
		{
			name: "Unblock:只有本人的黑名单",
			call: func(l *FriendLogic) (uint32, error) {
				r, err := l.Unblock(playerCtx(digestMe), &pb.UnblockRequest{TargetPlayerId: digestPeer})
				return r.GetErrorMessage().GetId(), err
			},
			want: map[string][]string{digestKey(digestMe): digestFields(domFriendBlock)},
		},
	}
	for _, c := range cases {
		t.Run(c.name, func(t *testing.T) {
			f := newFixture(t, nil)
			ev := f.withSyncDigest()
			if c.setup != nil {
				c.setup(f)
			}
			tip, err := c.call(f.logic())
			require.NoError(t, err)
			require.Zero(t, tip)
			assert.Equal(t, c.want, ev.bumped())
		})
	}
}

// 删一个本来就不是好友的人:repo 什么都不写,所以一个摘要都不许 bump ——
// 否则客户端能用任意 target 在核心 Redis 上造出 30 天的摘要键(见 listedAsFriend)。
func TestRemoveNonFriendDoesNotBump(t *testing.T) {
	f := newFixture(t, nil)
	ev := f.withSyncDigest()
	f.store.friends = []data.FriendEntry{{FriendPlayerID: 9999}}

	resp, err := f.logic().RemoveFriend(playerCtx(digestMe), &pb.RemoveFriendRequest{TargetPlayerId: digestPeer})
	require.NoError(t, err)
	require.Zero(t, resp.GetErrorMessage().GetId(), "删非好友是幂等成功")
	assert.Equal(t, 1, f.store.calls["RemoveFriend"], "判定只决定 bump,删除照常交给 repo")
	assert.Empty(t, ev.bumped())
}

// 删之前的那一读失败:拿不准就按「可能是好友」bump 双方(多拉一次,不漏),删除本身照常。
func TestRemoveFriendBumpsWhenPreReadFails(t *testing.T) {
	f := newFixture(t, nil)
	ev := f.withSyncDigest()
	f.store.fail("GetFriendList", errors.New("redis: connection refused"))

	resp, err := f.logic().RemoveFriend(playerCtx(digestMe), &pb.RemoveFriendRequest{TargetPlayerId: digestPeer})
	require.NoError(t, err)
	require.Zero(t, resp.GetErrorMessage().GetId(), "判定读失败不许把删好友变成失败")
	assert.Equal(t, 1, f.store.calls["RemoveFriend"])
	assert.Equal(t, map[string][]string{
		digestKey(digestMe):   digestFields(domFriendList),
		digestKey(digestPeer): digestFields(domFriendList),
	}, ev.bumped())
}

// 业务拒绝与存储故障:没有提交就不许 bump(§3.5 第 1 条)。
func TestRejectedWritesDoNotBump(t *testing.T) {
	// method 是假 store 上要失败的方法名,同时用来在 rpcs 里找对应的 RPC。
	cases := []struct {
		name   string
		method string
		err    error
	}{
		{"AddFriend 被拉黑", "AddFriendRequest", data.ErrBlocked},
		{"AcceptFriend 没有申请", "AcceptFriend", data.ErrRequestNotFound},
		{"RejectFriend 没有申请", "RejectFriend", data.ErrRequestNotFound},
		{"RemoveFriend 存储故障", "RemoveFriend", errors.New("i/o timeout")},
		{"Block 名单满", "Block", data.ErrBlockListFull},
		{"Unblock 存储故障", "Unblock", errors.New("i/o timeout")},
	}
	rpcs := map[string]func(l *FriendLogic) (uint32, error){
		"AddFriendRequest": func(l *FriendLogic) (uint32, error) {
			r, err := l.AddFriend(playerCtx(digestMe), &pb.AddFriendRequest{TargetPlayerId: digestPeer})
			return r.GetErrorMessage().GetId(), err
		},
		"AcceptFriend": func(l *FriendLogic) (uint32, error) {
			r, err := l.AcceptFriend(playerCtx(digestMe), &pb.AcceptFriendRequest{FromPlayerId: digestPeer})
			return r.GetErrorMessage().GetId(), err
		},
		"RejectFriend": func(l *FriendLogic) (uint32, error) {
			r, err := l.RejectFriend(playerCtx(digestMe), &pb.RejectFriendRequest{FromPlayerId: digestPeer})
			return r.GetErrorMessage().GetId(), err
		},
		"RemoveFriend": func(l *FriendLogic) (uint32, error) {
			r, err := l.RemoveFriend(playerCtx(digestMe), &pb.RemoveFriendRequest{TargetPlayerId: digestPeer})
			return r.GetErrorMessage().GetId(), err
		},
		"Block": func(l *FriendLogic) (uint32, error) {
			r, err := l.Block(playerCtx(digestMe), &pb.BlockRequest{TargetPlayerId: digestPeer})
			return r.GetErrorMessage().GetId(), err
		},
		"Unblock": func(l *FriendLogic) (uint32, error) {
			r, err := l.Unblock(playerCtx(digestMe), &pb.UnblockRequest{TargetPlayerId: digestPeer})
			return r.GetErrorMessage().GetId(), err
		},
	}
	for _, c := range cases {
		t.Run(c.name, func(t *testing.T) {
			f := newFixture(t, nil)
			ev := f.withSyncDigest()
			// RemoveFriend 要先过「是好友」的判定,才能证明不 bump 是因为删除失败而不是因为判成了非好友。
			f.store.friends = []data.FriendEntry{{FriendPlayerID: digestPeer}}
			f.store.fail(c.method, c.err)

			tip, err := rpcs[c.method](f.logic())
			require.NoError(t, err)
			require.NotZero(t, tip, "用例前提:这次写必须被拒或失败")
			assert.Empty(t, ev.bumped(), "没有提交就 bump,客户端会把旧数据记成新版本(L-2)")
		})
	}
}

// bump 失败不影响任何 RPC 的结果(§3.5 第 5 条):事务已经提交,漏掉的一次由面板拉取兜底。
func TestBumpFailureDoesNotAffectRpcResult(t *testing.T) {
	calls := map[string]func(l *FriendLogic) (uint32, error){
		"AddFriend": func(l *FriendLogic) (uint32, error) {
			r, err := l.AddFriend(playerCtx(digestMe), &pb.AddFriendRequest{TargetPlayerId: digestPeer})
			return r.GetErrorMessage().GetId(), err
		},
		"AcceptFriend": func(l *FriendLogic) (uint32, error) {
			r, err := l.AcceptFriend(playerCtx(digestMe), &pb.AcceptFriendRequest{FromPlayerId: digestPeer})
			return r.GetErrorMessage().GetId(), err
		},
		"RejectFriend": func(l *FriendLogic) (uint32, error) {
			r, err := l.RejectFriend(playerCtx(digestMe), &pb.RejectFriendRequest{FromPlayerId: digestPeer})
			return r.GetErrorMessage().GetId(), err
		},
		"RemoveFriend": func(l *FriendLogic) (uint32, error) {
			r, err := l.RemoveFriend(playerCtx(digestMe), &pb.RemoveFriendRequest{TargetPlayerId: digestPeer})
			return r.GetErrorMessage().GetId(), err
		},
		"Block": func(l *FriendLogic) (uint32, error) {
			r, err := l.Block(playerCtx(digestMe), &pb.BlockRequest{TargetPlayerId: digestPeer})
			return r.GetErrorMessage().GetId(), err
		},
		"Unblock": func(l *FriendLogic) (uint32, error) {
			r, err := l.Unblock(playerCtx(digestMe), &pb.UnblockRequest{TargetPlayerId: digestPeer})
			return r.GetErrorMessage().GetId(), err
		},
	}
	for name, call := range calls {
		t.Run(name, func(t *testing.T) {
			f := newFixture(t, nil)
			ev := f.withSyncDigest()
			ev.err = errors.New("dial tcp 127.0.0.1:6379: connect: connection refused")
			f.store.friends = []data.FriendEntry{{FriendPlayerID: digestPeer}}

			tip, err := call(f.logic())
			require.NoError(t, err)
			assert.Zero(t, tip, "摘要写不进去不许改写已提交的业务结果")
			assert.NotEmpty(t, ev.bumped(), "用例前提:确实尝试过 bump")
		})
	}
}

// 没装配摘要(SyncDigest = nil)的形态:全部写操作照常成功,不 panic。
func TestNilSyncDigestIsNoop(t *testing.T) {
	f := newFixture(t, nil) // SyncDigest 默认 nil
	require.Nil(t, f.svcCtx.SyncDigest)
	assert.NotPanics(t, func() {
		resp, err := f.logic().AcceptFriend(playerCtx(digestMe), &pb.AcceptFriendRequest{FromPlayerId: digestPeer})
		require.NoError(t, err)
		require.Zero(t, resp.GetErrorMessage().GetId())
	})
}

// 装配形态:SharedRedis(go-zero)上真实执行 BumpScript —— g 起种、计数加一、键带 TTL。
func TestAcceptFriendBumpsSharedRedis(t *testing.T) {
	f := newFixture(t, nil)
	f.svcCtx.SyncDigest = playersync.NewBumper(playersync.NewGoZeroEvaler(f.svcCtx.SharedRedis), "friend")

	resp, err := f.logic().AcceptFriend(playerCtx(digestMe), &pb.AcceptFriendRequest{FromPlayerId: digestPeer})
	require.NoError(t, err)
	require.Zero(t, resp.GetErrorMessage().GetId())

	for _, pid := range []uint64{digestMe, digestPeer} {
		key := digestKey(pid)
		assert.NotEmpty(t, f.sharedMr.HGet(key, "g"), "%s 应已起种代次", key)
		assert.Equal(t, "1", f.sharedMr.HGet(key, strconv.Itoa(int(domFriendList))), key)
		assert.Equal(t, "1", f.sharedMr.HGet(key, strconv.Itoa(int(domFriendRequest))), key)
		assert.Equal(t, playersync.DefaultTTL, f.sharedMr.TTL(key), key)
		assert.False(t, f.friendMr.Exists(key), "摘要键只能写在 SharedRedis(C++ 只读核心实例)")
	}
}
