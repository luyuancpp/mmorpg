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

// 父键已有真实存档:不覆盖、不加 TTL,建角照常成功,计 parent_exists(id 复用信号)。
func TestCreatePlayer_SeedNeverOverwritesExistingParent(t *testing.T) {
	setNewPlayerSeed(t, true)
	fake := &fakeRegisterClient{}
	h := newCreatePlayerHarnessWithNames(t, homezone.New(fake, 0, 0, 0), fake)
	existing, err := proto.Marshal(&pbdb.PlayerAllData{
		PlayerDatabaseData: &pbdb.PlayerDatabase{
			PlayerId:       hzPlayerID,
			LevelComponent: &comppb.LevelComp{Level: 30},
		},
	})
	if err != nil {
		t.Fatalf("marshal existing parent: %v", err)
	}
	if err := h.rdb.Set(h.ctx, seedParentKeyFor(hzPlayerID), existing, 0).Err(); err != nil {
		t.Fatalf("seed existing parent: %v", err)
	}
	before := seedCount(t, "parent_exists")

	resp, err := h.l.CreatePlayer(&loginpb.CreatePlayerRequest{})
	requireCreated(t, resp, err)

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

// owner_epoch 已铸(玩家被派入过场景):父键此刻不在也不写,建角照常成功,计 owner_epoch_exists。
func TestCreatePlayer_SeedRefusedAfterOwnerEpochMinted(t *testing.T) {
	setNewPlayerSeed(t, true)
	fake := &fakeRegisterClient{}
	h := newCreatePlayerHarnessWithNames(t, homezone.New(fake, 0, 0, 0), fake)
	if err := h.rdb.Set(h.ctx, ownerepoch.OwnerEpochKey(hzPlayerID), "3", 0).Err(); err != nil {
		t.Fatalf("seed owner_epoch: %v", err)
	}
	before := seedCount(t, "owner_epoch_exists")

	resp, err := h.l.CreatePlayer(&loginpb.CreatePlayerRequest{})
	requireCreated(t, resp, err)

	if h.mr.Exists(seedParentKeyFor(hzPlayerID)) {
		t.Fatal("seed written although player:{id}:owner_epoch exists")
	}
	if got := seedCount(t, "owner_epoch_exists") - before; got != 1 {
		t.Fatalf("seed_total{owner_epoch_exists} delta = %v, want 1", got)
	}
}

// 种子脚本报错(父键被预置成 list,脚本里的 GET 回 WRONGTYPE):建角照常成功,账号 blob 与反查索引照写,
// 计 error,父键原样不动。证明「种子结果不改变建角结果」对 error 分支同样成立。
func TestCreatePlayer_SeedErrorDoesNotFailCreate(t *testing.T) {
	setNewPlayerSeed(t, true)
	fake := &fakeRegisterClient{}
	h := newCreatePlayerHarnessWithNames(t, homezone.New(fake, 0, 0, 0), fake)
	key := seedParentKeyFor(hzPlayerID)
	if _, err := h.mr.Lpush(key, "x"); err != nil {
		t.Fatalf("seed list-typed parent key: %v", err)
	}
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
