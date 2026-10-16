package clientplayerloginlogic

import (
	"bytes"
	"context"
	"errors"
	"fmt"
	"strings"
	"testing"
	"time"

	"github.com/redis/go-redis/v9"
	"google.golang.org/protobuf/proto"

	"login/internal/config"
	"login/internal/constants"
	"login/internal/logic/pkg/dataloader"
	"login/internal/logic/pkg/homezone"
	pbbase "proto/common/base"
	comppb "proto/common/component"
	pbdb "proto/common/database"
	dspb "proto/data_service"
	loginpb "proto/login"
	"shared/generated/pb/table"
	"shared/idsegment"
	"shared/ownerepoch"
	"shared/playername"
)

// 建角写新号种子(docs/design/new-player-first-enter-no-cold-read.md)的单测。harness 沿用
// createplayer_homezone_test.go / createplayer_name_test.go / player_identity_backfill_test.go,
// 不新增假客户端:种子只碰 Redis,data_service 侧的调用序列必须与开关关着时一模一样。

// newPlayerSeedTTL 与 dataloader 里冷读组装父键的 TTL 相同(包内常量不导出,这里照抄数值)。
const newPlayerSeedTTL = 24 * time.Hour

// setNewPlayerSeed 设置种子开关,用例结束恢复原值。CreatePlayer 直接读全局 config.AppConfig,
// 不恢复会把同包后续用例悄悄变成「开着」。
func setNewPlayerSeed(t *testing.T, enabled bool) {
	t.Helper()
	saved := config.AppConfig.NewPlayerSeed
	config.AppConfig.NewPlayerSeed.Enabled = enabled
	t.Cleanup(func() { config.AppConfig.NewPlayerSeed = saved })
}

// seedCount 读 login_create_player_seed_total{result} 的当前值。计数是进程级累加的,用例只比增量,
// 而且要在动作**之前**先取一次:helper 内部才 Enable 指标开关,之前的写入都被丢弃了。
// result 写字面量而不是 SeedResult.String():这几个 label 值是告警表达式的契约,要单独钉住。
func seedCount(t *testing.T, result string) float64 {
	t.Helper()
	return createPlayerCounterValue(t, "login_create_player_seed_total", map[string]string{"result": result})
}

// seedParentKeyFor 与 backfill / scene / go/db 同一种拼法(PlayerAllData 全名 + player_id)。
func seedParentKeyFor(playerID uint64) string {
	return fmt.Sprintf("%s:%d", (&pbdb.PlayerAllData{}).ProtoReflect().Descriptor().FullName(), playerID)
}

// seedSubCacheKeys 是两张子表的缓存键:冷读发过读任务时 go/db 会回写它们,快路径下必须不存在。
func seedSubCacheKeys(playerID uint64) []string {
	return []string{
		fmt.Sprintf("%s:%d", (&pbdb.PlayerDatabase{}).ProtoReflect().Descriptor().FullName(), playerID),
		fmt.Sprintf("%s:%d", (&pbdb.PlayerDatabase_1{}).ProtoReflect().Descriptor().FullName(), playerID),
	}
}

// expectedSeed 是新号种子应有的内容:两张子表只带 player_id。在测试里独立写一份,不复用被测的构造。
func expectedSeed(playerID uint64) *pbdb.PlayerAllData {
	return &pbdb.PlayerAllData{
		PlayerDatabaseData:   &pbdb.PlayerDatabase{PlayerId: playerID},
		PlayerDatabase_1Data: &pbdb.PlayerDatabase_1{PlayerId: playerID},
	}
}

func readSeededParent(t *testing.T, ctx context.Context, rdb *redis.Client, playerID uint64) (*pbdb.PlayerAllData, []byte) {
	t.Helper()
	raw, err := rdb.Get(ctx, seedParentKeyFor(playerID)).Bytes()
	if err != nil {
		t.Fatalf("read parent key: %v", err)
	}
	parent := &pbdb.PlayerAllData{}
	if err := proto.Unmarshal(raw, parent); err != nil {
		t.Fatalf("unmarshal parent key: %v", err)
	}
	return parent, raw
}

func requireCreated(t *testing.T, resp *loginpb.CreatePlayerResponse, err error) {
	t.Helper()
	if err != nil {
		t.Fatalf("CreatePlayer: %v", err)
	}
	if resp.ErrorMessage != nil {
		t.Fatalf("unexpected tip %v", resp.ErrorMessage)
	}
}

// 开关关着:与本功能之前逐字节相同,不写父键。
func TestCreatePlayer_SeedOffWritesNoParentKey(t *testing.T) {
	setNewPlayerSeed(t, false)
	fake := &fakeRegisterClient{}
	h := newCreatePlayerHarnessWithNames(t, homezone.New(fake, 0, 0, 0), fake)

	resp, err := h.l.CreatePlayer(&loginpb.CreatePlayerRequest{})
	requireCreated(t, resp, err)

	if h.mr.Exists(seedParentKeyFor(hzPlayerID)) {
		t.Fatal("parent key written with NewPlayerSeed disabled")
	}
}

// 开关开着:父键 = 骨架、带 TTL;建角结果照旧;种子不碰 data_service(调用序列仍是 reserve,register)。
func TestCreatePlayer_SeedOnWritesSkeletonBeforeAccountBlob(t *testing.T) {
	setNewPlayerSeed(t, true)
	fake := &fakeRegisterClient{}
	h := newCreatePlayerHarnessWithNames(t, homezone.New(fake, 0, 0, 0), fake)
	writtenBefore := seedCount(t, "written")

	resp, err := h.l.CreatePlayer(&loginpb.CreatePlayerRequest{})
	requireCreated(t, resp, err)

	parent, _ := readSeededParent(t, h.ctx, h.rdb, hzPlayerID)
	if !proto.Equal(parent, expectedSeed(hzPlayerID)) {
		t.Fatalf("seed = %v, want the skeleton (player_id only on both sub tables)", parent)
	}
	if ttl := h.mr.TTL(seedParentKeyFor(hzPlayerID)); ttl <= 0 || ttl > newPlayerSeedTTL {
		t.Fatalf("seed TTL = %v, want (0, %v]", ttl, newPlayerSeedTTL)
	}
	if players := storedPlayers(t, h.ctx, h.rdb); len(players) != 1 || players[0].GetPlayerId() != hzPlayerID {
		t.Fatalf("persisted players = %v, want exactly player %d", players, hzPlayerID)
	}
	if got := seedCount(t, "written") - writtenBefore; got != 1 {
		t.Fatalf("seed_total{written} delta = %v, want 1", got)
	}
	if got := strings.Join(fake.events, ","); got != "reserve,register" {
		t.Fatalf("data_service call order = %q, want reserve,register (the seed must not touch data_service)", got)
	}
}

// 归属登记失败(fail-closed):建角整体失败,种子根本不写。
func TestCreatePlayer_SeedSkippedWhenHomeZoneRegistrationFails(t *testing.T) {
	setNewPlayerSeed(t, true)
	fake := &fakeRegisterClient{err: errors.New("down")}
	h := newCreatePlayerHarnessWithNames(t, homezone.New(fake, 0, 0, 0), fake)

	resp, err := h.l.CreatePlayer(&loginpb.CreatePlayerRequest{})
	if err != nil {
		t.Fatalf("CreatePlayer must fail via tip, not transport error: %v", err)
	}
	requireTip(t, resp, table.LoginError_kLoginDataSerializeFailed)

	if h.mr.Exists(seedParentKeyFor(hzPlayerID)) {
		t.Fatal("seed written although player:zone registration failed")
	}
}

// 种子写在账号 blob **之前**:建角锁在持锁期间丢了(在 Reserve 回调里删锁模拟过期后易主),
// 围栏写回 -1、回读确认不含新角色 → 建角失败;此时种子已经落地,只留下一份会过期的孤儿种子
// (这个 id 已经烧掉,没人能拿它进场)。做法照 TestCreatePlayerName_LostLockFencesAccountWrite。
func TestCreatePlayer_SeedSurvivesLostAccountWrite(t *testing.T) {
	setNewPlayerSeed(t, true)
	n := newNameHarness(t)
	n.fake.reserve = func(int, *dspb.ReservePlayerNameRequest) (*dspb.ReservePlayerNameResponse, error) {
		if err := n.rdb.Del(n.ctx, nameCreateKey).Err(); err != nil {
			t.Errorf("drop create lock: %v", err)
		}
		return &dspb.ReservePlayerNameResponse{Result: playername.ReserveOK}, nil
	}

	resp := n.create(t, "云中君")

	requireTip(t, resp, table.LoginError_kLoginInProgress)
	if players := storedPlayers(t, n.ctx, n.rdb); len(players) != 0 {
		t.Fatalf("account blob persisted %d player(s) although the fenced write was refused", len(players))
	}
	if ttl := n.mr.TTL(seedParentKeyFor(hzPlayerID)); ttl <= 0 {
		t.Fatalf("seed TTL = %v, want an expiring seed left behind (it is written before the account blob)", ttl)
	}
	if len(n.fake.releaseReqs) != 1 || len(n.scheduled) != 0 {
		t.Fatalf("release calls = %d, delayed = %d, want 1/0", len(n.fake.releaseReqs), len(n.scheduled))
	}
}

// idProbeCount 读 login_create_player_id_probe_total{result} 的当前值,口径同 seedCount:只比增量,
// 动作之前先取一次;result 写字面量,钉住告警表达式的契约。
func idProbeCount(t *testing.T, result string) float64 {
	t.Helper()
	return createPlayerCounterValue(t, "login_create_player_id_probe_total", map[string]string{"result": result})
}

// seedResultLabels 是 login_create_player_seed_total 的全部 result 取值,用来断言「种子根本没跑」。
var seedResultLabels = []string{"written", "idempotent", "parent_exists", "owner_epoch_exists", "error"}

// existingParentBlob 是一份「真实存档」形状的父键(有等级、不是种子骨架)。
func existingParentBlob(t *testing.T) []byte {
	t.Helper()
	existing, err := proto.Marshal(&pbdb.PlayerAllData{
		PlayerDatabaseData: &pbdb.PlayerDatabase{
			PlayerId:       hzPlayerID,
			LevelComponent: &comppb.LevelComp{Level: 30},
		},
	})
	if err != nil {
		t.Fatalf("marshal existing parent: %v", err)
	}
	return existing
}

// plantInSeedWindow 让 Reserve(6c)在回包之前执行 plant:状态出现在「6b' 探测之后、6d' 种子之前」的窗口里,
// 探测看不到它,只有种子脚本的写时复核能拦下。Reserve 本身照常回成功。
func plantInSeedWindow(t *testing.T, fake *fakeRegisterClient, plant func() error) {
	t.Helper()
	fake.reserve = func(int, *dspb.ReservePlayerNameRequest) (*dspb.ReservePlayerNameResponse, error) {
		if err := plant(); err != nil {
			t.Errorf("plant state in the probe→seed window: %v", err)
		}
		return &dspb.ReservePlayerNameResponse{Result: playername.ReserveOK}, nil
	}
}

// requireRefusedAtSeedAndCompensated 是种子报 id 复用(设计 §10 P2,2026-10-15 定为拒绝)之后的共同断言:
// 建角以 kLoginDataSerializeFailed 拒绝;名字(6c 确定已登记)按 reserve 时的 id / 名字释放恰好一次;
// 账号 blob 没有新角色、反查映射不写;建角锁已释放。6d 的 player:zone 已登记(撤不掉,与第 7 步失败同类),
// 体现在调用序列里的 register 上。
func requireRefusedAtSeedAndCompensated(t *testing.T, h *createPlayerHarness, fake *fakeRegisterClient,
	resp *loginpb.CreatePlayerResponse, err error) {
	t.Helper()
	if err != nil {
		t.Fatalf("CreatePlayer must fail via tip, not transport error: %v", err)
	}
	requireTip(t, resp, table.LoginError_kLoginDataSerializeFailed)
	if got := strings.Join(fake.events, ","); got != "reserve,register,release" {
		t.Fatalf("data_service call order = %q, want reserve,register,release", got)
	}
	reserved, released := fake.reserveReqs[0], fake.releaseReqs[0]
	if released.GetPlayerId() != reserved.GetPlayerId() || released.GetName() != reserved.GetName() {
		t.Fatalf("released %d/%q, want the reservation %d/%q",
			released.GetPlayerId(), released.GetName(), reserved.GetPlayerId(), reserved.GetName())
	}
	if players := storedPlayers(t, h.ctx, h.rdb); len(players) != 0 {
		t.Fatalf("account blob persisted %d player(s) although the create was refused", len(players))
	}
	if h.mr.Exists(constants.PlayerToAccountKey(hzPlayerID)) {
		t.Fatal("reverse mapping written although the create was refused")
	}
	if h.mr.Exists(nameCreateKey) {
		t.Fatal("create lock still held after a refused create")
	}
}

// 回归(设计 §10 P2,负责人 2026-10-15 按「最标准」定为拒绝):父键在「6b' 探测之后、6d' 种子之前」才出现
// (Reserve 回调里写入一份真实存档):种子脚本作为写时的原子复核,不覆盖、不加 TTL,计 parent_exists;
// 建角随之拒绝,名字释放一次。
//
// 期望改写(2026-10-15),逐条:
//   - 注入时机:原用例在建角之前就预置父键。现在 6b' 探测会在登记名字之前拒绝,种子根本跑不到,原写法测不到
//     种子脚本的「不覆盖」;所以挪到窗口里注入。建角之前就在的情形见 TestCreatePlayer_IDProbeRefusesMintedIDWithExistingState;
//   - 建角结果:原期望「建角照常成功」(只告警),改为 kLoginDataSerializeFailed 拒绝;
//   - 新增补偿断言:名字释放恰好一次、账号 blob 不变、反查映射不写、锁已释放;
//   - 保留不变:父键字节不变、TTL 仍为 0、seed_total{parent_exists} +1。
//
// 修复前:没有 6b' 探测,id_probe_total{clear} 不增长,前提断言处即红;即便去掉前提断言,种子报 parent_exists
// 也只告警、建角成功,requireTip 处同样红。
func TestCreatePlayer_SeedNeverOverwritesExistingParent(t *testing.T) {
	setNewPlayerSeed(t, true)
	fake := &fakeRegisterClient{}
	h := newCreatePlayerHarnessWithNames(t, homezone.New(fake, 0, 0, 0), fake)
	existing := existingParentBlob(t)
	plantInSeedWindow(t, fake, func() error {
		return h.rdb.Set(h.ctx, seedParentKeyFor(hzPlayerID), existing, 0).Err()
	})
	before := seedCount(t, "parent_exists")
	probeClearBefore := idProbeCount(t, "clear")

	resp, err := h.l.CreatePlayer(&loginpb.CreatePlayerRequest{})

	// 先钉住前提:探测确实放行了,拒绝来自种子那一步。
	if got := idProbeCount(t, "clear") - probeClearBefore; got != 1 {
		t.Fatalf("id_probe_total{clear} delta = %v, want 1 (the parent key must appear after the probe)", got)
	}
	requireRefusedAtSeedAndCompensated(t, h, fake, resp, err)
	if _, after := readSeededParent(t, h.ctx, h.rdb, hzPlayerID); !bytes.Equal(existing, after) {
		t.Fatal("seed overwrote an existing parent key")
	}
	if ttl := h.mr.TTL(seedParentKeyFor(hzPlayerID)); ttl != 0 {
		t.Fatalf("existing parent TTL = %v after create, want none", ttl)
	}
	if got := seedCount(t, "parent_exists") - before; got != 1 {
		t.Fatalf("seed_total{parent_exists} delta = %v, want 1", got)
	}
}

// 回归(设计 §10 P2,同上):owner_epoch 在「6b' 探测之后、6d' 种子之前」才被铸出(玩家被派入过场景):
// 父键此刻不在也不写,计 owner_epoch_exists;建角随之拒绝,名字释放一次。
//
// 期望改写(2026-10-15),逐条:
//   - 注入时机:原用例在建角之前就预置 owner_epoch,现在会被 6b' 探测先拦下,挪到窗口里注入;
//   - 建角结果:原期望「建角照常成功」,改为 kLoginDataSerializeFailed 拒绝;
//   - 新增补偿断言:同 TestCreatePlayer_SeedNeverOverwritesExistingParent;
//   - 保留不变:父键不存在、seed_total{owner_epoch_exists} +1。
//
// 修复前:没有 6b' 探测,前提断言处即红;即便去掉它,种子报 owner_epoch_exists 也只告警、建角成功,
// requireTip 处同样红。
func TestCreatePlayer_SeedRefusedAfterOwnerEpochMinted(t *testing.T) {
	setNewPlayerSeed(t, true)
	fake := &fakeRegisterClient{}
	h := newCreatePlayerHarnessWithNames(t, homezone.New(fake, 0, 0, 0), fake)
	plantInSeedWindow(t, fake, func() error {
		return h.rdb.Set(h.ctx, ownerepoch.OwnerEpochKey(hzPlayerID), "3", 0).Err()
	})
	before := seedCount(t, "owner_epoch_exists")
	probeClearBefore := idProbeCount(t, "clear")

	resp, err := h.l.CreatePlayer(&loginpb.CreatePlayerRequest{})

	// 先钉住前提:探测确实放行了,拒绝来自种子那一步。
	if got := idProbeCount(t, "clear") - probeClearBefore; got != 1 {
		t.Fatalf("id_probe_total{clear} delta = %v, want 1 (owner_epoch must appear after the probe)", got)
	}
	requireRefusedAtSeedAndCompensated(t, h, fake, resp, err)
	if h.mr.Exists(seedParentKeyFor(hzPlayerID)) {
		t.Fatal("seed written although player:{id}:owner_epoch exists")
	}
	if got := seedCount(t, "owner_epoch_exists") - before; got != 1 {
		t.Fatalf("seed_total{owner_epoch_exists} delta = %v, want 1", got)
	}
}

// 回归(设计 §10 P7,负责人 2026-10-15 按「最标准」定为采用):新铸 id 上建角之前就已有既有状态(父键 /
// owner_epoch)时,建角在登记名字**之前**拒绝(fail-closed):名字不登记、player:zone 不登记、种子不跑、
// 账号 blob 不变、反查映射不写,建角锁照常释放,计 id_probe_total{result} +1。种子开关开、关都要拒:
// id 复用在关着种子时同样会让新账号首次入场读到那份旧缓存。
//
// 修复前:没有探测。关着种子时建角直接成功;开着种子时种子报 parent_exists / owner_epoch_exists 只告警、
// 建角照常成功,名字与 player:zone 都已登记 —— 两种都在 requireTip 处即红。
func TestCreatePlayer_IDProbeRefusesMintedIDWithExistingState(t *testing.T) {
	plantParent := func(t *testing.T, h *createPlayerHarness) {
		t.Helper()
		if err := h.rdb.Set(h.ctx, seedParentKeyFor(hzPlayerID), existingParentBlob(t), 0).Err(); err != nil {
			t.Fatalf("plant existing parent: %v", err)
		}
	}
	plantOwnerEpoch := func(t *testing.T, h *createPlayerHarness) {
		t.Helper()
		if err := h.rdb.Set(h.ctx, ownerepoch.OwnerEpochKey(hzPlayerID), "3", 0).Err(); err != nil {
			t.Fatalf("plant owner_epoch: %v", err)
		}
	}
	cases := []struct {
		name   string
		seed   bool // NewPlayerSeed.Enabled
		plant  func(*testing.T, *createPlayerHarness)
		result string // id_probe_total 的 result
	}{
		{"父键已在,种子开", true, plantParent, "parent_exists"},
		{"父键已在,种子关", false, plantParent, "parent_exists"},
		{"owner_epoch 已在,种子开", true, plantOwnerEpoch, "owner_epoch_exists"},
		{"owner_epoch 已在,种子关", false, plantOwnerEpoch, "owner_epoch_exists"},
	}
	for _, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			setNewPlayerSeed(t, tc.seed)
			fake := &fakeRegisterClient{}
			h := newCreatePlayerHarnessWithNames(t, homezone.New(fake, 0, 0, 0), fake)
			tc.plant(t, h)
			seedKey := seedParentKeyFor(hzPlayerID)
			parentBefore, parentErrBefore := h.mr.Get(seedKey)
			probeBefore := idProbeCount(t, tc.result)
			seedBefore := make(map[string]float64, len(seedResultLabels))
			for _, label := range seedResultLabels {
				seedBefore[label] = seedCount(t, label)
			}

			resp, err := h.l.CreatePlayer(&loginpb.CreatePlayerRequest{})

			if err != nil {
				t.Fatalf("CreatePlayer must fail via tip, not transport error: %v", err)
			}
			requireTip(t, resp, table.LoginError_kLoginDataSerializeFailed)
			// 名字、player:zone 一个 RPC 都没发,也就没有释放。
			if got := strings.Join(fake.events, ","); got != "" {
				t.Fatalf("data_service calls = %q, want none (refused before reserving the name)", got)
			}
			if got := idProbeCount(t, tc.result) - probeBefore; got != 1 {
				t.Fatalf("id_probe_total{%s} delta = %v, want 1", tc.result, got)
			}
			for _, label := range seedResultLabels {
				if got := seedCount(t, label) - seedBefore[label]; got != 0 {
					t.Fatalf("seed_total{%s} delta = %v, want 0 (the seed must not run)", label, got)
				}
			}
			if parentAfter, parentErrAfter := h.mr.Get(seedKey); parentAfter != parentBefore ||
				(parentErrBefore == nil) != (parentErrAfter == nil) {
				t.Fatalf("parent key changed: %q/%v -> %q/%v", parentBefore, parentErrBefore, parentAfter, parentErrAfter)
			}
			if ttl := h.mr.TTL(seedKey); ttl != 0 {
				t.Fatalf("parent TTL = %v after a refused create, want none", ttl)
			}
			if players := storedPlayers(t, h.ctx, h.rdb); len(players) != 0 {
				t.Fatalf("account blob persisted %d player(s) although the create was refused", len(players))
			}
			if h.mr.Exists(constants.PlayerToAccountKey(hzPlayerID)) {
				t.Fatal("reverse mapping written although the create was refused")
			}
			if h.mr.Exists(nameCreateKey) {
				t.Fatal("create lock still held after a refused create")
			}
		})
	}
}

// mintHookSegment 发出号之后立刻跑一次 after:把故障注入卡在「发号之后、6b' 探测之前」。
type mintHookSegment struct {
	id    uint64
	after func()
}

func (s *mintHookSegment) Next(context.Context) (uint64, error) {
	s.after()
	return s.id, nil
}

// 回归(设计 §10 P7 的 fail-closed 一半):探测本身出错(Redis 在发号之后整个不可用)⇒ 拒绝建角,名字、
// player:zone 一个都不登记,不记孤儿,计 id_probe_total{error} +1。探不清不等于没有:放行就可能把一个
// 在役 id 交给新账号。
//
// 修复前:没有探测,名字与 player:zone 照常登记;随后围栏写与回读都失败,按「分不清」保留名字登记、记一个孤儿,
// tip 是 kLoginRedisSetFailed —— requireTip 处即红(调用序列也会是 reserve,register)。
func TestCreatePlayer_IDProbeErrorRefusesCreate(t *testing.T) {
	n := newNameHarness(t)
	n.l.svcCtx.PlayerIDMinter = &idsegment.Minter{Name: "player_id", Segment: &mintHookSegment{id: hzPlayerID, after: func() {
		n.mr.SetError("simulated redis outage")
	}}}
	t.Cleanup(func() { n.mr.SetError("") })
	errorsBefore := idProbeCount(t, "error")
	orphansBefore := orphanCount(t)

	resp := n.create(t, "云中君")
	// 后面的断言要经 go-redis 读回账号 blob。建角锁的释放发生在故障期间,本用例不对它断言。
	n.mr.SetError("")

	requireTip(t, resp, table.LoginError_kLoginDataSerializeFailed)
	if got := strings.Join(n.fake.events, ","); got != "" {
		t.Fatalf("data_service calls = %q, want none (refused before reserving the name)", got)
	}
	if got := idProbeCount(t, "error") - errorsBefore; got != 1 {
		t.Fatalf("id_probe_total{error} delta = %v, want 1", got)
	}
	if got := orphanCount(t) - orphansBefore; got != 0 {
		t.Fatalf("orphan_total delta = %v, want 0 (no name was reserved)", got)
	}
	if players := storedPlayers(t, n.ctx, n.rdb); len(players) != 0 {
		t.Fatalf("account blob persisted %d player(s) although the probe failed", len(players))
	}
	if n.mr.Exists(constants.PlayerToAccountKey(hzPlayerID)) || n.mr.Exists(seedParentKeyFor(hzPlayerID)) {
		t.Fatal("reverse mapping or seed written although the probe failed")
	}
}

// 种子脚本报错(父键是个 list,脚本里的 GET 回 WRONGTYPE):建角照常成功,账号 blob 与反查索引照写,
// 计 error,父键原样不动。证明「种子结果不改变建角结果」对 error 分支同样成立。
// 2026-10-15 改注入时机:list 键在 Reserve 回调里(6b' 探测之后)才写入。原先建角之前就预置,现在会被探测
// 当成 parent_exists 拒在 6c 之前,种子根本跑不到;期望不变。
func TestCreatePlayer_SeedErrorDoesNotFailCreate(t *testing.T) {
	setNewPlayerSeed(t, true)
	fake := &fakeRegisterClient{}
	h := newCreatePlayerHarnessWithNames(t, homezone.New(fake, 0, 0, 0), fake)
	key := seedParentKeyFor(hzPlayerID)
	plantInSeedWindow(t, fake, func() error {
		_, err := h.mr.Lpush(key, "x")
		return err
	})
	before := seedCount(t, "error")

	resp, err := h.l.CreatePlayer(&loginpb.CreatePlayerRequest{})
	requireCreated(t, resp, err)

	if players := storedPlayers(t, h.ctx, h.rdb); len(players) != 1 || players[0].GetPlayerId() != hzPlayerID {
		t.Fatalf("persisted players = %v, want exactly player %d", players, hzPlayerID)
	}
	if got, err := h.rdb.Get(h.ctx, constants.PlayerToAccountKey(hzPlayerID)).Result(); err != nil || got != hzAccount {
		t.Fatalf("reverse mapping = %q/%v, want %q", got, err, hzAccount)
	}
	if got := seedCount(t, "error") - before; got != 1 {
		t.Fatalf("seed_total{error} delta = %v, want 1", got)
	}
	if typ := h.mr.Type(key); typ != "list" {
		t.Fatalf("parent key type = %q after a failed seed, want the original list untouched", typ)
	}
}

// 种子 → EnterGame:预加载走 EXISTS 快路径(svc 里 KafkaClient / TaskResultDispatcher 为 nil,
// 任何冷读分支都会 panic,链就走不到 SetSession),身份补齐写进种子、保留种子的 TTL,
// 没有子表缓存回写。
func TestEnterGame_SeededNewPlayerTakesFastPath(t *testing.T) {
	h := newEnterWiringHarness(t, []*pbbase.AccountSimplePlayer{{PlayerId: identityPlayerID, ClassId: 3, Name: "云中君"}}, nil)
	ctx := context.Background()
	// harness 默认种下一份老存档;换成建角种子,模拟「刚建完角、第一次进场」。
	if err := h.rdb.Del(ctx, identityBlobKey()).Err(); err != nil {
		t.Fatalf("drop harness parent key: %v", err)
	}
	if got, err := dataloader.SeedNewPlayerAllData(ctx, h.rdb, identityPlayerID); err != nil || got != dataloader.SeedWritten {
		t.Fatalf("SeedNewPlayerAllData = %v / err %v, want written / nil", got, err)
	}

	resp, err := h.l.EnterGame(&loginpb.EnterGameRequest{PlayerId: identityPlayerID})
	if err != nil {
		t.Fatalf("EnterGame: %v", err)
	}
	if resp.GetErrorMessage() != nil {
		t.Fatalf("EnterGame rejected with tip %d, want the async chain started", resp.GetErrorMessage().GetId())
	}
	h.waitChain(t)

	if got := h.locator.setSessionCalls.Load(); got != 1 {
		t.Fatalf("SetSession calls = %d, want 1 (the chain must take the fast path and get past identity backfill)", got)
	}
	raw, player := readIdentityBlob(t, h.rdb)
	if got := player.GetUint32PbComponent().GetClass(); got != 3 {
		t.Fatalf("class = %d, want 3", got)
	}
	if got := player.GetProfileComponent().GetName(); got != "云中君" {
		t.Fatalf("profile name = %q, want 云中君", got)
	}
	parent := &pbdb.PlayerAllData{}
	if err := proto.Unmarshal(raw, parent); err != nil {
		t.Fatalf("unmarshal parent key: %v", err)
	}
	if got := parent.GetPlayerDatabase_1Data().GetPlayerId(); got != identityPlayerID {
		t.Fatalf("player_database_1 player_id = %d, want %d (backfill must keep the seed's second sub table)", got, identityPlayerID)
	}
	if ttl, err := h.rdb.PTTL(ctx, identityBlobKey()).Result(); err != nil || ttl <= 0 || ttl > newPlayerSeedTTL {
		t.Fatalf("parent PTTL = %v / err %v, want (0, %v] (backfill keeps the seed TTL)", ttl, err, newPlayerSeedTTL)
	}
	if n, err := h.rdb.Exists(ctx, seedSubCacheKeys(identityPlayerID)...).Result(); err != nil || n != 0 {
		t.Fatalf("sub cache keys present = %d / err %v, want 0 (no DB read on the fast path)", n, err)
	}
}
