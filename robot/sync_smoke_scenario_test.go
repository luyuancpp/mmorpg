package main

// sync-smoke 里纯函数的单测:不连服务器,只喂构造出来的摘要与留底。
// 冒烟本身要等整套服务起来才跑得到,而它的判定函数若写反了(该红不红),联机时是看不出来的 ——
// 所以每个判定函数都要有「合法形状放行 / 每种坏形状各自被拦」两面。

import (
	"errors"
	"maps"
	"slices"
	"testing"

	"google.golang.org/protobuf/proto"

	"proto/scene"
	"robot/generated/pb/game"
)

const (
	syncTestFriendList    = scene.SyncDomain_SYNC_DOMAIN_FRIEND_LIST
	syncTestFriendRequest = scene.SyncDomain_SYNC_DOMAIN_FRIEND_REQUEST
	syncTestFriendBlock   = scene.SyncDomain_SYNC_DOMAIN_FRIEND_BLOCK
	syncTestTeam          = scene.SyncDomain_SYNC_DOMAIN_TEAM
)

// syncTestDigest 构造一份 status=OK 的摘要;versions 为 nil 时等于"没有任何域"。
func syncTestDigest(generation uint64, versions map[scene.SyncDomain]uint64) syncSmokeDigest {
	if versions == nil {
		versions = map[scene.SyncDomain]uint64{}
	}
	return syncSmokeDigest{
		status:     scene.SyncDigestStatus_SYNC_DIGEST_STATUS_OK,
		generation: generation,
		versions:   versions,
	}
}

func syncTestUnavailable() syncSmokeDigest {
	return syncSmokeDigest{
		status:   scene.SyncDigestStatus_SYNC_DIGEST_STATUS_UNAVAILABLE,
		versions: map[scene.SyncDomain]uint64{},
	}
}

func syncTestRecord(t *testing.T, messageId uint32, envelopeTip uint32, body proto.Message) travelSmokeRecord {
	t.Helper()
	record := travelSmokeRecord{messageId: messageId, envelopeTip: envelopeTip}
	if body != nil {
		raw, err := proto.Marshal(body)
		if err != nil {
			t.Fatalf("marshal: %v", err)
		}
		record.body = raw
	}
	return record
}

func TestSyncSmokeRegisteredDomainsAreSortedAndExcludeUnspecified(t *testing.T) {
	domains := syncSmokeRegisteredDomains()
	if len(domains) == 0 {
		t.Fatal("no registered domains")
	}
	if !slices.IsSorted(domains) {
		t.Fatalf("domains not sorted: %v", domains)
	}
	if slices.Contains(domains, scene.SyncDomain_SYNC_DOMAIN_UNSPECIFIED) {
		t.Fatalf("UNSPECIFIED must not be registered: %v", domains)
	}
	for _, want := range []scene.SyncDomain{syncTestFriendList, syncTestFriendRequest, syncTestFriendBlock, syncTestTeam} {
		if !slices.Contains(domains, want) {
			t.Fatalf("missing domain %s in %v", want, domains)
		}
	}
	if len(domains) != len(scene.SyncDomain_name)-1 {
		t.Fatalf("registered %d domains, enum has %d values (minus UNSPECIFIED)", len(domains), len(scene.SyncDomain_name)-1)
	}
}

func TestSyncSmokeParseDigestAcceptsLegalShapes(t *testing.T) {
	cases := []struct {
		name string
		push *scene.SyncDigestS2C
		want syncSmokeDigest
	}{
		{"digest absent", &scene.SyncDigestS2C{}, syncTestDigest(0, nil)},
		{"generation without entries", &scene.SyncDigestS2C{Generation: 7}, syncTestDigest(7, nil)},
		{"entries", &scene.SyncDigestS2C{Generation: 7, Entries: []*scene.SyncDigestEntry{
			{Domain: syncTestFriendRequest, Version: 3},
			{Domain: syncTestFriendBlock, Version: 1},
		}}, syncTestDigest(7, map[scene.SyncDomain]uint64{syncTestFriendRequest: 3, syncTestFriendBlock: 1})},
		// 新服务端先上、生成枚举还没跟上时会出现不认识的正域号:保留,不当成坏形状。
		{"unknown positive domain kept", &scene.SyncDigestS2C{Generation: 7, Entries: []*scene.SyncDigestEntry{
			{Domain: scene.SyncDomain(9999), Version: 2},
		}}, syncTestDigest(7, map[scene.SyncDomain]uint64{scene.SyncDomain(9999): 2})},
	}
	for _, c := range cases {
		t.Run(c.name, func(t *testing.T) {
			got, err := syncSmokeParseDigest(c.push)
			if err != nil {
				t.Fatalf("unexpected error: %v", err)
			}
			if got.status != c.want.status || got.generation != c.want.generation || !maps.Equal(got.versions, c.want.versions) {
				t.Fatalf("got %s, want %s", syncSmokeDescribe(got), syncSmokeDescribe(c.want))
			}
		})
	}
}

func TestSyncSmokeParseDigestRejectsMalformed(t *testing.T) {
	cases := []struct {
		name string
		push *scene.SyncDigestS2C
	}{
		{"unspecified domain", &scene.SyncDigestS2C{Generation: 7, Entries: []*scene.SyncDigestEntry{
			{Domain: scene.SyncDomain_SYNC_DOMAIN_UNSPECIFIED, Version: 1},
		}}},
		{"negative domain", &scene.SyncDigestS2C{Generation: 7, Entries: []*scene.SyncDigestEntry{
			{Domain: scene.SyncDomain(-1), Version: 1},
		}}},
		{"duplicated domain", &scene.SyncDigestS2C{Generation: 7, Entries: []*scene.SyncDigestEntry{
			{Domain: syncTestFriendRequest, Version: 1},
			{Domain: syncTestFriendRequest, Version: 2},
		}}},
		{"entries without generation", &scene.SyncDigestS2C{Entries: []*scene.SyncDigestEntry{
			{Domain: syncTestFriendRequest, Version: 1},
		}}},
	}
	for _, c := range cases {
		t.Run(c.name, func(t *testing.T) {
			if _, err := syncSmokeParseDigest(c.push); err == nil {
				t.Fatal("expected an error")
			}
		})
	}
}

// UNAVAILABLE 时 generation / entries 没有意义,不校验形状,只把 status 交给调用方判。
func TestSyncSmokeParseDigestSkipsShapeChecksWhenUnavailable(t *testing.T) {
	got, err := syncSmokeParseDigest(&scene.SyncDigestS2C{
		Status:  scene.SyncDigestStatus_SYNC_DIGEST_STATUS_UNAVAILABLE,
		Entries: []*scene.SyncDigestEntry{{Domain: scene.SyncDomain_SYNC_DOMAIN_UNSPECIFIED}},
	})
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if got.status != scene.SyncDigestStatus_SYNC_DIGEST_STATUS_UNAVAILABLE {
		t.Fatalf("status=%s, want UNAVAILABLE", got.status)
	}
}

func TestSyncSmokeDiffTreatsAbsentAsZeroAndSortsByDomain(t *testing.T) {
	before := syncTestDigest(7, map[scene.SyncDomain]uint64{syncTestTeam: 1, syncTestFriendList: 0, syncTestFriendBlock: 2})
	after := syncTestDigest(7, map[scene.SyncDomain]uint64{syncTestTeam: 3, syncTestFriendRequest: 1, syncTestFriendBlock: 2})
	got := syncSmokeDiff(before, after)
	want := []syncSmokeDomainChange{
		{domain: syncTestFriendRequest, before: 0, after: 1},
		{domain: syncTestTeam, before: 1, after: 3},
	}
	if !slices.Equal(got, want) {
		t.Fatalf("got %s, want %s", syncSmokeDescribeChanges(got), syncSmokeDescribeChanges(want))
	}
	if changes := syncSmokeDiff(before, before); len(changes) != 0 {
		t.Fatalf("identical digests must not differ: %s", syncSmokeDescribeChanges(changes))
	}
}

func TestSyncSmokeCheckGenerationStable(t *testing.T) {
	cases := []struct {
		name          string
		before, after uint64
		wantErr       bool
	}{
		{"absent stays absent", 0, 0, false},
		{"hash created", 0, 9, false},
		{"unchanged", 9, 9, false},
		{"hash disappeared", 9, 0, true},
		{"hash rebuilt", 9, 10, true},
	}
	for _, c := range cases {
		t.Run(c.name, func(t *testing.T) {
			err := syncSmokeCheckGenerationStable(syncTestDigest(c.before, nil), syncTestDigest(c.after, nil))
			if (err != nil) != c.wantErr {
				t.Fatalf("err=%v, wantErr=%t", err, c.wantErr)
			}
		})
	}
}

func TestSyncSmokeCheckUnchanged(t *testing.T) {
	base := syncTestDigest(7, map[scene.SyncDomain]uint64{syncTestFriendRequest: 3, syncTestFriendBlock: 1})
	if err := syncSmokeCheckUnchanged(base, syncTestDigest(7, map[scene.SyncDomain]uint64{syncTestFriendRequest: 3, syncTestFriendBlock: 1})); err != nil {
		t.Fatalf("identical digests rejected: %v", err)
	}
	// 显式的 0 与缺席等价。
	if err := syncSmokeCheckUnchanged(base, syncTestDigest(7, map[scene.SyncDomain]uint64{syncTestFriendRequest: 3, syncTestFriendBlock: 1, syncTestTeam: 0})); err != nil {
		t.Fatalf("explicit zero must equal absent: %v", err)
	}
	if err := syncSmokeCheckUnchanged(syncTestDigest(0, nil), syncTestDigest(0, nil)); err != nil {
		t.Fatalf("absent -> absent rejected: %v", err)
	}
	bad := []struct {
		name          string
		before, after syncSmokeDigest
	}{
		{"count changed", base, syncTestDigest(7, map[scene.SyncDomain]uint64{syncTestFriendRequest: 4, syncTestFriendBlock: 1})},
		{"domain appeared", base, syncTestDigest(7, map[scene.SyncDomain]uint64{syncTestFriendRequest: 3, syncTestFriendBlock: 1, syncTestTeam: 1})},
		{"generation changed", base, syncTestDigest(8, map[scene.SyncDomain]uint64{syncTestFriendRequest: 3, syncTestFriendBlock: 1})},
		{"digest disappeared", base, syncTestDigest(0, nil)},
		{"digest created", syncTestDigest(0, nil), syncTestDigest(5, map[scene.SyncDomain]uint64{syncTestTeam: 1})},
	}
	for _, c := range bad {
		t.Run(c.name, func(t *testing.T) {
			if err := syncSmokeCheckUnchanged(c.before, c.after); err == nil {
				t.Fatal("expected an error")
			}
		})
	}
}

func TestSyncSmokeCheckSingleBumpAcceptsExactlyOneIncrement(t *testing.T) {
	before := syncTestDigest(7, map[scene.SyncDomain]uint64{syncTestFriendRequest: 3, syncTestFriendBlock: 1})
	after := syncTestDigest(7, map[scene.SyncDomain]uint64{syncTestFriendRequest: 4, syncTestFriendBlock: 1})
	if err := syncSmokeCheckSingleBump(before, after, syncTestFriendRequest); err != nil {
		t.Fatalf("legal single bump rejected: %v", err)
	}
	// 摘要此前不存在:第一次 bump 建 hash、写入新的 g,该域从 0 到 1。
	created := syncTestDigest(5, map[scene.SyncDomain]uint64{syncTestFriendRequest: 1})
	if err := syncSmokeCheckSingleBump(syncTestDigest(0, nil), created, syncTestFriendRequest); err != nil {
		t.Fatalf("hash creation by the first bump rejected: %v", err)
	}
}

func TestSyncSmokeCheckSingleBumpReportsNotYet(t *testing.T) {
	before := syncTestDigest(7, map[scene.SyncDomain]uint64{syncTestFriendRequest: 3})
	if err := syncSmokeCheckSingleBump(before, before, syncTestFriendRequest); !errors.Is(err, errSyncSmokeNotBumpedYet) {
		t.Fatalf("err=%v, want errSyncSmokeNotBumpedYet", err)
	}
	if err := syncSmokeCheckSingleBump(syncTestDigest(0, nil), syncTestDigest(0, nil), syncTestFriendRequest); !errors.Is(err, errSyncSmokeNotBumpedYet) {
		t.Fatalf("absent -> absent: err=%v, want errSyncSmokeNotBumpedYet", err)
	}
}

func TestSyncSmokeCheckSingleBumpRejectsWrongShapes(t *testing.T) {
	before := syncTestDigest(7, map[scene.SyncDomain]uint64{syncTestFriendRequest: 3, syncTestFriendBlock: 1})
	cases := []struct {
		name  string
		after syncSmokeDigest
	}{
		{"other domain also changed", syncTestDigest(7, map[scene.SyncDomain]uint64{syncTestFriendRequest: 4, syncTestFriendBlock: 2})},
		{"only another domain changed", syncTestDigest(7, map[scene.SyncDomain]uint64{syncTestFriendRequest: 3, syncTestFriendBlock: 2})},
		{"new domain appeared", syncTestDigest(7, map[scene.SyncDomain]uint64{syncTestFriendRequest: 4, syncTestFriendBlock: 1, syncTestTeam: 1})},
		{"incremented by two", syncTestDigest(7, map[scene.SyncDomain]uint64{syncTestFriendRequest: 5, syncTestFriendBlock: 1})},
		{"went down", syncTestDigest(7, map[scene.SyncDomain]uint64{syncTestFriendRequest: 2, syncTestFriendBlock: 1})},
		{"generation changed", syncTestDigest(8, map[scene.SyncDomain]uint64{syncTestFriendRequest: 4, syncTestFriendBlock: 1})},
		{"digest disappeared", syncTestDigest(0, nil)},
	}
	for _, c := range cases {
		t.Run(c.name, func(t *testing.T) {
			err := syncSmokeCheckSingleBump(before, c.after, syncTestFriendRequest)
			if err == nil {
				t.Fatal("expected an error")
			}
			if errors.Is(err, errSyncSmokeNotBumpedYet) {
				t.Fatalf("wrong shape must not be reported as not-yet: %v", err)
			}
		})
	}
}

func TestSyncSmokeDirtyDomains(t *testing.T) {
	registered := []scene.SyncDomain{syncTestFriendList, syncTestFriendRequest, syncTestFriendBlock, syncTestTeam}
	cached := syncTestDigest(7, map[scene.SyncDomain]uint64{syncTestFriendRequest: 3, syncTestFriendBlock: 1})
	cases := []struct {
		name   string
		cached *syncSmokeDigest
		fresh  syncSmokeDigest
		want   []scene.SyncDomain
	}{
		{"no cache pulls everything", nil, cached, registered},
		{"unavailable pulls everything", &cached, syncTestUnavailable(), registered},
		{"generation changed pulls everything",
			&cached, syncTestDigest(8, map[scene.SyncDomain]uint64{syncTestFriendRequest: 3, syncTestFriendBlock: 1}), registered},
		{"only changed domain",
			&cached, syncTestDigest(7, map[scene.SyncDomain]uint64{syncTestFriendRequest: 4, syncTestFriendBlock: 1}),
			[]scene.SyncDomain{syncTestFriendRequest}},
		// 比较用「不相等」:计数变小(回档)同样要拉。
		{"count went down is still dirty",
			&cached, syncTestDigest(7, map[scene.SyncDomain]uint64{syncTestFriendRequest: 2, syncTestFriendBlock: 1}),
			[]scene.SyncDomain{syncTestFriendRequest}},
		{"newly present domain",
			&cached, syncTestDigest(7, map[scene.SyncDomain]uint64{syncTestFriendRequest: 3, syncTestFriendBlock: 1, syncTestTeam: 1}),
			[]scene.SyncDomain{syncTestTeam}},
		{"nothing changed", &cached, cached, nil},
		// 不在注册集合里的域(机器人枚举还没跟上的新域)不参与待拉取计算。
		{"unregistered domain ignored",
			&cached, syncTestDigest(7, map[scene.SyncDomain]uint64{syncTestFriendRequest: 3, syncTestFriendBlock: 1, scene.SyncDomain(9999): 1}),
			nil},
	}
	for _, c := range cases {
		t.Run(c.name, func(t *testing.T) {
			got := syncSmokeDirtyDomains(c.cached, c.fresh, registered)
			if !slices.Equal(got, c.want) {
				t.Fatalf("got %s, want %s", syncSmokeDomainList(got), syncSmokeDomainList(c.want))
			}
		})
	}
	// 全量拉取返回的是副本:调用方改它不能改到注册集合。
	all := syncSmokeDirtyDomains(nil, cached, registered)
	all[0] = syncTestTeam
	if registered[0] != syncTestFriendList {
		t.Fatal("dirty set aliases the registered slice")
	}
}

func TestSyncSmokeClassifyProbe(t *testing.T) {
	enter := syncTestRecord(t, game.SceneSceneClientPlayerNotifyEnterSceneMessageId, 0, &scene.EnterSceneS2C{})
	okReply := syncTestRecord(t, game.SceneSceneClientPlayerNotifySyncDigestMessageId, 0, &scene.SyncDigestS2C{Generation: 7})
	unavailableReply := syncTestRecord(t, game.SceneSceneClientPlayerNotifySyncDigestMessageId, 0,
		&scene.SyncDigestS2C{Status: scene.SyncDigestStatus_SYNC_DIGEST_STATUS_UNAVAILABLE})
	envelope := syncTestRecord(t, game.SceneSceneClientPlayerGetSyncDigestMessageId, tipSyncRateLimited, nil)
	kick := syncTestRecord(t, game.SceneClientPlayerCommonKickPlayerMessageId, 0, nil)
	undecodable := travelSmokeRecord{messageId: game.SceneSceneClientPlayerNotifySyncDigestMessageId, body: []byte{0xff, 0xff, 0xff}}

	cases := []struct {
		name    string
		records []travelSmokeRecord
		want    syncSmokeProbe
	}{
		{"nothing arrived", nil, syncSmokeProbe{outcome: syncSmokeProbeDropped}},
		{"only enter scene", []travelSmokeRecord{enter}, syncSmokeProbe{outcome: syncSmokeProbeDropped}},
		{"reply after enter scene", []travelSmokeRecord{enter, okReply},
			syncSmokeProbe{outcome: syncSmokeProbeRepliedOK}},
		{"reply before enter scene", []travelSmokeRecord{okReply, enter},
			syncSmokeProbe{outcome: syncSmokeProbeRepliedOK, replyBeforeEnterScene: true}},
		{"unavailable reply", []travelSmokeRecord{enter, unavailableReply},
			syncSmokeProbe{outcome: syncSmokeProbeRepliedUnavailable}},
		{"envelope rejected", []travelSmokeRecord{envelope, enter},
			syncSmokeProbe{outcome: syncSmokeProbeEnvelopeRejected, envelopeTip: tipSyncRateLimited}},
		// 第一个决定性的结局为准:信封拒绝之后迟到的 254(不可能来自同一请求)不改写结论。
		{"first outcome wins", []travelSmokeRecord{envelope, okReply},
			syncSmokeProbe{outcome: syncSmokeProbeEnvelopeRejected, envelopeTip: tipSyncRateLimited}},
		{"undecodable reply", []travelSmokeRecord{undecodable},
			syncSmokeProbe{outcome: syncSmokeProbeUndecodable, replyBeforeEnterScene: true}},
		// kicked 扫完全部留底,不因结局已定而停。
		{"kicked after reply", []travelSmokeRecord{enter, okReply, kick},
			syncSmokeProbe{outcome: syncSmokeProbeRepliedOK, kicked: true}},
		// 253 不带信封拒绝的回包(scene 对 Empty 方法本不回包)不算结局。
		{"plain 253 ignored", []travelSmokeRecord{syncTestRecord(t, game.SceneSceneClientPlayerGetSyncDigestMessageId, 0, nil)},
			syncSmokeProbe{outcome: syncSmokeProbeDropped}},
	}
	for _, c := range cases {
		t.Run(c.name, func(t *testing.T) {
			if got := syncSmokeClassifyProbe(c.records); got != c.want {
				t.Fatalf("got %+v, want %+v", got, c.want)
			}
		})
	}
}

func TestSyncSmokeDescribeIsDeterministic(t *testing.T) {
	digest := syncTestDigest(7, map[scene.SyncDomain]uint64{syncTestTeam: 1, syncTestFriendRequest: 3})
	if got, want := syncSmokeDescribe(digest), "{g=7 FRIEND_REQUEST=3 TEAM=1}"; got != want {
		t.Fatalf("got %q, want %q", got, want)
	}
	if got, want := syncSmokeDescribe(syncTestUnavailable()), "{status=SYNC_DIGEST_STATUS_UNAVAILABLE}"; got != want {
		t.Fatalf("got %q, want %q", got, want)
	}
	if got, want := syncSmokeDomainList(nil), "[]"; got != want {
		t.Fatalf("got %q, want %q", got, want)
	}
	changes := []syncSmokeDomainChange{{domain: syncTestFriendRequest, before: 3, after: 4}}
	if got, want := syncSmokeDescribeChanges(changes), "[FRIEND_REQUEST 3->4]"; got != want {
		t.Fatalf("got %q, want %q", got, want)
	}
}
