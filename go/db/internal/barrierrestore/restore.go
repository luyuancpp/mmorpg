// Package barrierrestore 在停写维护期间从权威 SQL 恢复历史玩家的提交屏障。
// 旧恢复模式不改归属/缓存；显式promotion仅在审定撤销计划中提升围栏并清同SQL的缓存。
// 两种模式都不能替代停旧写者与审定 Kafka/retry 的运维门禁。
package barrierrestore

import (
	"bytes"
	"context"
	"crypto/rand"
	"crypto/sha256"
	"database/sql"
	"encoding/binary"
	"encoding/hex"
	"encoding/json"
	"errors"
	"fmt"
	"google.golang.org/protobuf/encoding/protojson"
	"io"
	"net"
	"os"
	"path/filepath"
	assetpb "proto/common/asset"
	base "proto/common/base"
	dspb "proto/data_service"
	"shared/accountdirectory"
	"sort"
	"strconv"
	"strings"
	"time"

	databasepb "proto/common/database"
	"shared/ownerepoch"
	"shared/placement"
	"shared/savebarrier"

	"github.com/redis/go-redis/v9"
	"google.golang.org/protobuf/proto"
	"google.golang.org/protobuf/reflect/protoreflect"
)

const Format = "player-save-barrier-restore-v1"
const FormatV2 = "player-save-barrier-restore-v2"
const RoutingModeRedis = "redis-same-instance"
const RoutingModeSplit = "redis-split"
const RoutingModeDBFirst = "db_first"
const PromotionFormat = "player-save-fence-promotion-v1"

// Plan 只包含身份与摘要。它不是停写证明，执行时仍须独立维持维护窗口。
//
// AllowHomeFallback(2026-10-20 修复，r2-mz#1)：显式允许「没有落点记录、home == Zone、Schema 就是本区 zone 库」的玩家
// 按 go/db 的有效落点规则(记录 ?? home 的 zone 库)回退，不再要求先钉稳定落点；copy 合服 / 撤销后的玩家天然如此。
// 只在计划阶段由运维开启并随计划进 SHA-256；缺省 false 时行为与之前完全一致，旧计划照常可读。见 homeZoneFallback。
type Plan struct {
	Format                 string   `json:"format"`
	Schema                 string   `json:"schema"`
	Zone                   uint32   `json:"home_zone"`
	MaintenanceID          string   `json:"maintenance_id"`
	AllowLegacy            bool     `json:"allow_legacy"`
	RedisAddr              string   `json:"redis_addr"`
	RedisDB                int      `json:"redis_db"`
	RoutingMode            string   `json:"routing_mode"`
	LocalMySQL             bool     `json:"local_mysql"`
	MappingAddr            string   `json:"mapping_addr,omitempty"`
	MappingDB              int      `json:"mapping_db,omitempty"`
	RouteSchema            string   `json:"route_schema,omitempty"`
	AuditedOwnerUpperBound string   `json:"audited_owner_upper_bound,omitempty"`
	AllowHomeFallback      bool     `json:"allow_home_fallback,omitempty"`
	Players                []Player `json:"players"`
}

type Player struct {
	ID              string    `json:"player_id"`
	Owner           string    `json:"owner_epoch"`
	Original        string    `json:"original_version"`
	Target          string    `json:"target_version"`
	Rows            [2]string `json:"asset_row_sha256"`
	HomeZone        string    `json:"home_zone_value"`
	Placement       string    `json:"placement_value"`
	RouteGeneration string    `json:"route_generation,omitempty"`
	RouteWatermark  string    `json:"route_watermark,omitempty"`
	RouteRevision   string    `json:"route_revision,omitempty"`
}

// Restorer 的两个依赖均为维护专用连接，不能与正在接流量的业务池共用。
type Restorer struct {
	// 仅新promotion使用，由审定CLI封套钉住本地实例marker；旧恢复模式不读取它。
	LocalTargetID          string
	RedisTargetID          string
	DB                     *sql.DB
	Redis                  *redis.Client
	RoutingMode            string
	LocalMySQL             bool
	MappingRedis           *redis.Client
	RouteSchema            string
	AuditedOwnerUpperBound string
	// AllowHomeFallback 只由 Prepare 读取并写进计划(Plan.AllowHomeFallback);执行以审定计划为准,CLI 在 apply 模式拒绝该开关。
	AllowHomeFallback bool
}

// PreparePromotion 只读生成单玩家审定原件。停写证据摘要必须独立审定，不能由当前SQL猜U。
// 新维护分支仅支持同一TiDB上的db_first；SQL事务内不访问Redis/Scene/Kafka。
func (r Restorer) PreparePromotion(ctx context.Context, schema string, zone uint32, maintenanceID, id string, evidence []byte) (*databasepb.SaveBarrierMaintenanceReceipt_PromotionPlan, error) {
	pid, err := positive(id)
	if err != nil || r.DB == nil || r.Redis == nil || r.RoutingMode != RoutingModeDBFirst || len(evidence) != 32 || !promotionTargetID(r.LocalTargetID) || !promotionTargetID(r.RedisTargetID) {
		return nil, errors.New("fence-only提升范围、本地实例或审核证据无效")
	}
	p := Plan{Format: FormatV2, Schema: schema, Zone: zone, MaintenanceID: maintenanceID, RoutingMode: RoutingModeDBFirst, RedisAddr: r.Redis.Options().Addr, RedisDB: r.Redis.Options().DB, MappingAddr: r.mapping().Options().Addr, MappingDB: r.mapping().Options().DB, RouteSchema: r.RouteSchema}
	if err := p.ValidateTarget(); err != nil {
		return nil, err
	}
	u, err := auditedBound(r.AuditedOwnerUpperBound)
	if err != nil {
		return nil, err
	}
	ownerRaw, err := r.Redis.Get(ctx, ownerepoch.OwnerEpochKey(pid)).Result()
	if err != nil {
		return nil, errors.New("原owner缺失")
	}
	owner, err := positive(ownerRaw)
	if err != nil {
		return nil, err
	}
	prior, err := r.Redis.HGet(ctx, savebarrier.Key(pid), "handoff_fence").Result()
	if errors.Is(err, redis.Nil) {
		prior = ""
	} else if err != nil {
		return nil, err
	}
	cache, err := r.readPromotionCache(ctx, zone, pid)
	if err != nil {
		return nil, err
	}
	tx, err := r.DB.BeginTx(ctx, &sql.TxOptions{Isolation: sql.LevelReadCommitted})
	if err != nil {
		return nil, err
	}
	defer tx.Rollback()
	if err := r.checkPromotionSQLTarget(ctx, tx, p); err != nil {
		return nil, err
	}
	row, err := r.lockRoute(ctx, tx, p, id)
	if err != nil {
		return nil, err
	}
	pair, err := readPair(ctx, tx, schema, id)
	if err != nil {
		return nil, err
	}
	if err := tx.Rollback(); err != nil {
		return nil, err
	}
	if pair[0].schemaGen != pair[1].schemaGen || owner < pair[0].epoch {
		return nil, errors.New("原SQL世代不同或owner落后")
	}
	if err := verifyPromotionCache(cache, recoveryKeys(zone, pid), pair, pid); err != nil {
		return nil, err
	}
	max := owner
	if u > max {
		max = u
	}
	if max == ^uint64(0) {
		return nil, errors.New("owner提升溢出")
	}
	var nonce [16]byte
	if _, err := rand.Read(nonce[:]); err != nil {
		return nil, err
	}
	token := hex.EncodeToString(nonce[:])
	revoked, err := promotedRevocation(prior, owner, max+1, token)
	if err != nil {
		return nil, err
	}
	storage, _ := placement.StorageIDFromDBName(schema)
	placementVersion := uint64(0)
	if row.Placement != "" {
		record, err := placement.Parse(row.Placement)
		if err != nil {
			return nil, err
		}
		placementVersion = record.Version
	}
	rev, _ := positive(row.RouteRevision)
	watermark, _ := positive(row.RouteWatermark)
	one, _ := hex.DecodeString(pair[0].digest)
	two, _ := hex.DecodeString(pair[1].digest)
	plan := &databasepb.SaveBarrierMaintenanceReceipt_PromotionPlan{Format: PromotionFormat, MaintenanceId: maintenanceID, PlayerId: pid, RowSchema: schema, RouteSchema: r.RouteSchema, HomeZone: zone, StorageId: storage, RouteRevision: rev, RouteGeneration: row.RouteGeneration, RouteWatermark: watermark, PlacementVersion: placementVersion, AuditedOwnerUpperBound: u, OriginalOwner: owner, TargetOwner: max + 1, OriginalSaveEpoch: pair[0].epoch, OriginalSaveSeq: pair[0].seq, TargetSaveSeq: 1, SchemaGen: pair[0].schemaGen, FirstBusinessSha256: one, SecondBusinessSha256: two, OriginalFence: prior, RevokedFence: revoked, ReceiptId: []byte(token), RedisAddr: p.RedisAddr, RedisDb: int32(p.RedisDB), MappingAddr: p.MappingAddr, MappingDb: int32(p.MappingDB), AuditEvidenceSha256: bytes.Clone(evidence), WritersStopped: true, QueuesDrained: true}
	for _, key := range promotionCacheKeys(zone, pid) {
		value, present := cache[key]
		kind := "string"
		if key == recoveryKeys(zone, pid)[5] {
			kind = "hash"
		}
		observation := &databasepb.SaveBarrierMaintenanceReceipt_PromotionPlan_CacheObservation{Key: key, Present: present, RedisType: kind}
		if present {
			sum := sha256.Sum256(value)
			observation.Sha256 = sum[:]
		}
		plan.Caches = append(plan.Caches, observation)
	}
	if err := ValidatePromotion(plan); err != nil {
		return nil, err
	}
	if err := r.inspectMapping(ctx, p, row); err != nil {
		return nil, err
	}
	if err := r.runPromotionScript(ctx, plan, cache, "check"); err != nil {
		return nil, err
	}
	return plan, nil
}

func promotionCacheKeys(zone uint32, player uint64) []string {
	keys := recoveryKeys(zone, player)
	return []string{keys[2], keys[3], keys[4], keys[5]}
}

func promotedRevocation(prior string, originalOwner, target uint64, token string) (string, error) {
	validNonce := func(s string, zero bool) bool {
		return len(s) == 32 && strings.Trim(s, "0123456789abcdef") == "" && (zero || s != strings.Repeat("0", 32))
	}
	if !validNonce(token, false) {
		return "", errors.New("撤销nonce无效")
	}
	if prior == "" {
		return fmt.Sprintf("%d||0|%s|R|%s", target, strings.Repeat("0", 32), token), nil
	}
	original, err := savebarrier.ParseHandoffFence(prior)
	if err != nil || original.OwnerEpoch > originalOwner || original.Token == token {
		return "", errors.New("原handoff围栏不规范")
	}
	return fmt.Sprintf("%d|%s|%d|%s|R|%s", original.OwnerEpoch, original.SceneInstance, original.Generation, original.AdmissionToken, token), nil
}

func promotionRoute(p *databasepb.SaveBarrierMaintenanceReceipt_PromotionPlan) (Plan, Player) {
	row := Player{ID: strconv.FormatUint(p.PlayerId, 10), HomeZone: strconv.FormatUint(uint64(p.HomeZone), 10), RouteRevision: strconv.FormatUint(p.RouteRevision, 10), RouteGeneration: p.RouteGeneration, RouteWatermark: strconv.FormatUint(p.RouteWatermark, 10)}
	if p.PlacementVersion != 0 {
		row.Placement = placement.StableValue(p.StorageId, p.PlacementVersion)
	}
	return Plan{Format: FormatV2, Schema: p.RowSchema, Zone: p.HomeZone, MaintenanceID: p.MaintenanceId, RoutingMode: RoutingModeDBFirst, RedisAddr: p.RedisAddr, RedisDB: int(p.RedisDb), MappingAddr: p.MappingAddr, MappingDB: int(p.MappingDb), RouteSchema: p.RouteSchema}, row
}

// ValidatePromotion 用同一wire类型校验CLI与执行计划；cache条目按完整键核唯一和全集，顺序无意义。
func ValidatePromotion(p *databasepb.SaveBarrierMaintenanceReceipt_PromotionPlan) error {
	if p == nil || len(p.ProtoReflect().GetUnknown()) != 0 || p.Format != PromotionFormat || p.PlayerId == 0 || len(p.MaintenanceId) == 0 || len(p.MaintenanceId) > 128 || !p.WritersStopped || !p.QueuesDrained || len(p.AuditEvidenceSha256) != 32 || len(p.FirstBusinessSha256) != 32 || len(p.SecondBusinessSha256) != 32 {
		return errors.New("提升计划契约无效")
	}
	max := p.OriginalOwner
	if p.AuditedOwnerUpperBound > max {
		max = p.AuditedOwnerUpperBound
	}
	if max == ^uint64(0) || p.TargetOwner != max+1 || p.OriginalOwner == 0 || p.OriginalSaveEpoch > p.OriginalOwner || (p.OriginalSaveEpoch == 0) != (p.OriginalSaveSeq == 0) || p.TargetSaveSeq != 1 {
		return errors.New("提升计划版本无效")
	}
	target, row := promotionRoute(p)
	if err := target.ValidateTarget(); err != nil {
		return err
	}
	if err := validatePlannedRoute(target, row); err != nil {
		return err
	}
	storage, ok := placement.StorageIDFromDBName(p.RowSchema)
	if !ok || storage != p.StorageId {
		return errors.New("提升计划落点不符")
	}
	revoked, err := promotedRevocation(p.OriginalFence, p.OriginalOwner, p.TargetOwner, string(p.ReceiptId))
	if err != nil || revoked != p.RevokedFence {
		return errors.New("提升计划R原件不符")
	}
	keys := map[string]bool{}
	for _, key := range promotionCacheKeys(p.HomeZone, p.PlayerId) {
		keys[key] = false
	}
	if len(p.Caches) != len(keys) {
		return errors.New("缓存观察全集不符")
	}
	assetKey := recoveryKeys(p.HomeZone, p.PlayerId)[5]
	for _, entry := range p.Caches {
		if entry == nil || len(entry.ProtoReflect().GetUnknown()) != 0 {
			return errors.New("缓存观察损坏")
		}
		seen, ok := keys[entry.Key]
		kind := "string"
		if entry.Key == assetKey {
			kind = "hash"
		}
		if !ok || seen || entry.RedisType != kind || entry.Present && len(entry.Sha256) != 32 || !entry.Present && len(entry.Sha256) != 0 {
			return errors.New("缓存观察键/类型/摘要不符")
		}
		keys[entry.Key] = true
	}
	return nil
}

// PromotionDigest 对规范PB原件做域分离摘要；SQL永久收据存的正是这些bytes。
func PromotionDigest(p *databasepb.SaveBarrierMaintenanceReceipt_PromotionPlan) ([]byte, []byte, error) {
	if err := ValidatePromotion(p); err != nil {
		return nil, nil, err
	}
	raw, err := (proto.MarshalOptions{Deterministic: true}).Marshal(p)
	if err != nil {
		return nil, nil, err
	}
	h := sha256.New()
	writePromotionPart(h, []byte(PromotionFormat))
	writePromotionPart(h, raw)
	return raw, h.Sum(nil), nil
}

func writePromotionPart(h interface{ Write([]byte) (int, error) }, v []byte) {
	var n [8]byte
	binary.BigEndian.PutUint64(n[:], uint64(len(v)))
	_, _ = h.Write(n[:])
	_, _ = h.Write(v)
}

func (r Restorer) readPromotionCache(ctx context.Context, zone uint32, player uint64) (map[string][]byte, error) {
	keys := recoveryKeys(zone, player)
	out := map[string][]byte{}
	for _, key := range keys[2:5] {
		raw, err := r.Redis.Get(ctx, key).Bytes()
		if errors.Is(err, redis.Nil) {
			continue
		}
		if err != nil {
			return nil, err
		}
		if len(raw) == 0 || len(raw) > 16<<20 {
			return nil, errors.New("缓存bytes预算或形状无效")
		}
		out[key] = raw
	}
	kind, err := r.Redis.Type(ctx, keys[5]).Result()
	if err != nil {
		return nil, err
	}
	if kind != "none" {
		if kind != "hash" {
			return nil, errors.New("资产证据类型无效")
		}
		evidence, err := r.Redis.HGetAll(ctx, keys[5]).Result()
		if err != nil {
			return nil, err
		}
		if len(evidence) != 2 || len(evidence["version"]) != 41 || len(evidence["data"]) == 0 || len(evidence["data"]) > 16<<20 {
			return nil, errors.New("资产证据字段不完整")
		}
		out[keys[5]] = []byte(evidence["version"] + evidence["data"])
	}
	return out, nil
}

func verifyPromotionCache(cache map[string][]byte, keys []string, rows [2]sqlRow, player uint64) error {
	check := func(raw []byte, msg proto.Message, row sqlRow) error {
		if proto.Unmarshal(raw, msg) != nil || len(msg.ProtoReflect().GetUnknown()) != 0 {
			return errors.New("缓存PB未知顶层字段或损坏，不能证明无丢内容")
		}
		fields := msg.ProtoReflect().Descriptor().Fields()
		for name, rawSQL := range row.values {
			if name == "fence_owner_epoch" || name == "fence_save_seq" {
				continue
			}
			field := fields.ByName(protoreflect.Name(name))
			if field == nil {
				return errors.New("SQL存在缓存模型无法解释的列，拒绝清缓存")
			}
			value := msg.ProtoReflect().Get(field)
			switch field.Kind() {
			case protoreflect.MessageKind:
				fromSQL := value.Message().New().Interface()
				if proto.Unmarshal(rawSQL, fromSQL) != nil || (!msg.ProtoReflect().Has(field) && proto.Size(fromSQL) != 0) || (msg.ProtoReflect().Has(field) && !proto.Equal(fromSQL, value.Message().Interface())) {
					return errors.New("缓存与SQL消息内容不同")
				}
			case protoreflect.Uint64Kind, protoreflect.Uint32Kind, protoreflect.Fixed64Kind, protoreflect.Fixed32Kind:
				if string(rawSQL) != strconv.FormatUint(value.Uint(), 10) {
					return errors.New("缓存与SQL标量不同")
				}
			case protoreflect.StringKind:
				if string(rawSQL) != value.String() {
					return errors.New("缓存与SQL字符串不同")
				}
			case protoreflect.BytesKind:
				if !bytes.Equal(rawSQL, value.Bytes()) {
					return errors.New("缓存与SQL原bytes不同")
				}
			default:
				return errors.New("缓存SQL列类型尚无安全比较契约")
			}
		}
		// 不允许SQL缺已知业务列；NULL消息是空消息，不是漏列。
		for i := 0; i < fields.Len(); i++ {
			if _, ok := row.values[string(fields.Get(i).Name())]; !ok {
				return errors.New("缓存字段无对应SQL列")
			}
		}
		return nil
	}
	if raw, ok := cache[keys[2]]; ok {
		parent := &databasepb.PlayerAllData{}
		if proto.Unmarshal(raw, parent) != nil || len(parent.ProtoReflect().GetUnknown()) != 0 || parent.GetLastSaveSeq() != rows[0].seq || parent.PlayerDatabaseData == nil || parent.PlayerDatabase_1Data == nil || parent.GetPlayerDatabaseData().GetPlayerId() != player || parent.GetPlayerDatabase_1Data().GetPlayerId() != player {
			return errors.New("父缓存身份/版本无法证明")
		}
		one, _ := proto.Marshal(parent.PlayerDatabaseData)
		two, _ := proto.Marshal(parent.PlayerDatabase_1Data)
		if err := check(one, &databasepb.PlayerDatabase{}, rows[0]); err != nil {
			return err
		}
		if err := check(two, &databasepb.PlayerDatabase_1{}, rows[1]); err != nil {
			return err
		}
		if parent.MailData != nil || parent.BagData != nil && !proto.Equal(parent.BagData, parent.PlayerDatabaseData.BagComponent) || parent.QuestData != nil && !proto.Equal(parent.QuestData, parent.PlayerDatabaseData.MissionComponent) {
			return errors.New("父缓存含未承载SQL的额外内容")
		}
	}
	for i, key := range keys[3:5] {
		if raw, ok := cache[key]; ok {
			var msg proto.Message = &databasepb.PlayerDatabase{}
			if i == 1 {
				msg = &databasepb.PlayerDatabase_1{}
			}
			if err := check(raw, msg, rows[i]); err != nil {
				return err
			}
		}
	}
	if raw, ok := cache[keys[5]]; ok {
		evidence := &assetpb.PlayerAssetSQLCommit{}
		if len(raw) < 41 || string(raw[:41]) != savebarrier.Version(rows[0].epoch, rows[0].seq) || proto.Unmarshal(raw[41:], evidence) != nil || len(evidence.ProtoReflect().GetUnknown()) != 0 || evidence.PlayerId != player || evidence.OwnerEpoch != rows[0].epoch || evidence.SaveSeq != rows[0].seq {
			return errors.New("资产证据不是原SQL同版")
		}
		// 账本bytes来自同一SQL列；不重新盖旧版本证据，也不丢弃未知账本字段。
		ledger, ok := rows[0].values["asset_op_ledger"]
		if !ok || !bytes.Equal(ledger, evidence.LedgerProto) {
			return errors.New("资产证据账本与SQL原bytes不同")
		}
	}
	return nil
}

func (r Restorer) runPromotionScript(ctx context.Context, p *databasepb.SaveBarrierMaintenanceReceipt_PromotionPlan, cache map[string][]byte, mode string) error {
	keys := recoveryKeys(p.HomeZone, p.PlayerId)
	gen := p.SchemaGen
	if gen == 0 {
		gen = 1
	}
	args := []any{p.MaintenanceId, strconv.FormatUint(p.OriginalOwner, 10), strconv.FormatUint(p.TargetOwner, 10), savebarrier.Version(p.OriginalSaveEpoch, p.OriginalSaveSeq), savebarrier.Version(p.TargetOwner, p.TargetSaveSeq), strconv.FormatUint(uint64(gen), 10), p.OriginalFence, p.RevokedFence, mode}
	observations := map[string]*databasepb.SaveBarrierMaintenanceReceipt_PromotionPlan_CacheObservation{}
	for _, entry := range p.Caches {
		observations[entry.Key] = entry
	}
	for _, key := range keys[2:5] {
		raw, present := cache[key]
		entry := observations[key]
		flag := "0"
		if entry.Present {
			flag = "1"
		}
		if present {
			sum := sha256.Sum256(raw)
			if !entry.Present || !bytes.Equal(sum[:], entry.Sha256) {
				return errors.New("缓存偏离审定原件")
			}
		}
		args = append(args, flag, string(raw))
	}
	asset, present := cache[keys[5]]
	entry := observations[keys[5]]
	flag := "0"
	if entry.Present {
		flag = "1"
	}
	if present {
		sum := sha256.Sum256(asset)
		if !entry.Present || len(asset) < 41 || !bytes.Equal(sum[:], entry.Sha256) {
			return errors.New("资产证据偏离审定原件")
		}
		args = append(args, flag, string(asset[:41]), string(asset[41:]))
	} else {
		args = append(args, flag, "", "")
	}
	// 永久实例nonce在实际Lua提交点复核；run_id/replid只作CLI当次观测，AOF重启不换计划。
	keys = append(keys[:72:72], "debug_import:local_target", "debug_import:local_instance_id")
	args = append(args, r.RedisTargetID)
	n, err := promotionScript.Run(ctx, r.Redis, keys, args...).Int()
	if err != nil {
		return fmt.Errorf("维护撤销或屏障确认拒绝: %w", err)
	}
	if n != 1 {
		return errors.New("维护脚本返回未知结果")
	}
	return nil
}

// ApplyPromotion 的各SQL阶段均不调用Redis。永久R+更高owner先撤销旧B5资格，
// 再以SQL永久阶段原件引用撤销证明；双行只改围栏元数据。未知提交保持维护并重放同一nonce。
func (r Restorer) ApplyPromotion(ctx context.Context, p *databasepb.SaveBarrierMaintenanceReceipt_PromotionPlan) (*databasepb.SaveBarrierMaintenanceReceipt, error) {
	raw, digest, err := PromotionDigest(p)
	if err != nil {
		return nil, err
	}
	if r.DB == nil || r.Redis == nil || !promotionTargetID(r.LocalTargetID) || !promotionTargetID(r.RedisTargetID) || r.RoutingMode != RoutingModeDBFirst || r.RouteSchema != p.RouteSchema || r.Redis.Options().Addr != p.RedisAddr || r.Redis.Options().DB != int(p.RedisDb) || r.mapping().Options().Addr != p.MappingAddr || r.mapping().Options().DB != int(p.MappingDb) || strconv.FormatUint(p.AuditedOwnerUpperBound, 10) != r.AuditedOwnerUpperBound {
		return nil, errors.New("实际维护目标或旧写上界偏离审定计划")
	}
	if _, _, err = r.promotionSQL(ctx, p, raw, digest, databasepb.SaveBarrierMaintenanceReceipt_PLAN_FIXED); err != nil {
		return nil, err
	}
	phase, pair, err := r.promotionSQL(ctx, p, raw, digest, databasepb.SaveBarrierMaintenanceReceipt_PHASE_UNKNOWN)
	if err != nil {
		return nil, err
	}
	if phase == databasepb.SaveBarrierMaintenanceReceipt_PLAN_FIXED {
		cache, err := r.readPromotionCache(ctx, p.HomeZone, p.PlayerId)
		if err != nil {
			return nil, err
		}
		if err := verifyPromotionCache(cache, recoveryKeys(p.HomeZone, p.PlayerId), pair, p.PlayerId); err != nil {
			return nil, err
		}
		target, row := promotionRoute(p)
		if err := r.inspectMapping(ctx, target, row); err != nil {
			return nil, err
		}
		if err := r.runPromotionScript(ctx, p, cache, "revoke"); err != nil {
			return nil, err
		}
		phase, _, err = r.promotionSQL(ctx, p, raw, digest, databasepb.SaveBarrierMaintenanceReceipt_WRITERS_REVOKED)
		if err != nil {
			return nil, err
		}
	}
	if phase == databasepb.SaveBarrierMaintenanceReceipt_WRITERS_REVOKED {
		// R确认已永久记录。后续只做SQL操作，绝不在角色/路由/双行锁内调网络。
		phase, _, err = r.promotionSQL(ctx, p, raw, digest, databasepb.SaveBarrierMaintenanceReceipt_SQL_PROMOTED)
		if err != nil {
			return nil, err
		}
	}
	if phase == databasepb.SaveBarrierMaintenanceReceipt_SQL_PROMOTED {
		cache, err := r.readPromotionCache(ctx, p.HomeZone, p.PlayerId)
		if err != nil {
			return nil, err
		}
		target, row := promotionRoute(p)
		if err := r.inspectMapping(ctx, target, row); err != nil {
			return nil, err
		}
		if err := r.runPromotionScript(ctx, p, cache, "finish"); err != nil {
			return nil, err
		}
		phase, _, err = r.promotionSQL(ctx, p, raw, digest, databasepb.SaveBarrierMaintenanceReceipt_BARRIER_CONFIRMED)
		if err != nil {
			return nil, err
		}
	}
	if phase != databasepb.SaveBarrierMaintenanceReceipt_BARRIER_CONFIRMED {
		return nil, errors.New("维护收据阶段未知")
	}
	out := &databasepb.SaveBarrierMaintenanceReceipt{ReceiptId: bytes.Clone(p.ReceiptId), PlayerId: p.PlayerId, RequestDigest: digest, PlanProto: raw, Phase: phase}
	if err := r.DB.QueryRowContext(ctx, "SELECT updated_ms FROM `"+p.RouteSchema+"`.save_barrier_maintenance_receipt WHERE receipt_id=?", p.ReceiptId).Scan(&out.UpdatedMs); err != nil {
		return nil, err
	}
	return out, nil
}

func promotionTargetID(value string) bool {
	return len(value) == 32 && strings.Trim(value, "0123456789abcdef") == "" && value != strings.Repeat("0", 32)
}

// next=UNKNOWN只读取；阶段只允许相邻推进，原件永不改写。无隐式重试、无事务内外部I/O。
func (r Restorer) promotionSQL(ctx context.Context, p *databasepb.SaveBarrierMaintenanceReceipt_PromotionPlan, raw, digest []byte, next databasepb.SaveBarrierMaintenanceReceipt_Phase) (databasepb.SaveBarrierMaintenanceReceipt_Phase, [2]sqlRow, error) {
	unknown := databasepb.SaveBarrierMaintenanceReceipt_PHASE_UNKNOWN
	var pair [2]sqlRow
	tx, err := r.DB.BeginTx(ctx, &sql.TxOptions{Isolation: sql.LevelReadCommitted})
	if err != nil {
		return unknown, pair, err
	}
	defer tx.Rollback()
	target, row := promotionRoute(p)
	if err := r.checkPromotionSQLTarget(ctx, tx, target); err != nil {
		return unknown, pair, err
	}
	if err := r.recheckRoute(ctx, tx, target, row); err != nil {
		return unknown, pair, err
	}
	pair, err = readPair(ctx, tx, p.RowSchema, row.ID)
	if err != nil {
		return unknown, pair, err
	}
	for i, want := range [][]byte{p.FirstBusinessSha256, p.SecondBusinessSha256} {
		if pair[i].digest != hex.EncodeToString(want) || pair[i].schemaGen != p.SchemaGen {
			return unknown, pair, errors.New("原业务全列或世代发生变化")
		}
	}
	table := "`" + p.RouteSchema + "`.save_barrier_maintenance_receipt"
	var player uint64
	var savedRaw, savedDigest []byte
	var phase databasepb.SaveBarrierMaintenanceReceipt_Phase
	err = tx.QueryRowContext(ctx, "SELECT player_id,request_digest,plan_proto,phase FROM "+table+" WHERE receipt_id=? FOR UPDATE", p.ReceiptId).Scan(&player, &savedDigest, &savedRaw, &phase)
	if errors.Is(err, sql.ErrNoRows) {
		if next != databasepb.SaveBarrierMaintenanceReceipt_PLAN_FIXED || pair[0].epoch != p.OriginalSaveEpoch || pair[0].seq != p.OriginalSaveSeq {
			return unknown, pair, errors.New("维护原件缺席或原SQL版本变化")
		}
		_, err = tx.ExecContext(ctx, "INSERT INTO "+table+"(receipt_id,player_id,request_digest,plan_proto,phase,updated_ms) VALUES(?,?,?,?,?,CAST(UNIX_TIMESTAMP(NOW(3))*1000 AS UNSIGNED))", p.ReceiptId, p.PlayerId, digest, raw, databasepb.SaveBarrierMaintenanceReceipt_PLAN_FIXED)
		if err != nil {
			return unknown, pair, err
		}
		if err = tx.Commit(); err != nil {
			return unknown, pair, fmt.Errorf("维护计划提交结局未知，保持维护并仅原计划重读: %w", err)
		}
		return databasepb.SaveBarrierMaintenanceReceipt_PLAN_FIXED, pair, nil
	}
	if err != nil {
		return unknown, pair, err
	}
	if player != p.PlayerId || !bytes.Equal(savedRaw, raw) || !bytes.Equal(savedDigest, digest) || phase < databasepb.SaveBarrierMaintenanceReceipt_PLAN_FIXED || phase > databasepb.SaveBarrierMaintenanceReceipt_BARRIER_CONFIRMED {
		return unknown, pair, errors.New("维护永久原件冲突或未知阶段")
	}
	expectE, expectQ := p.OriginalSaveEpoch, p.OriginalSaveSeq
	if phase >= databasepb.SaveBarrierMaintenanceReceipt_SQL_PROMOTED {
		expectE, expectQ = p.TargetOwner, p.TargetSaveSeq
	}
	if pair[0].epoch != expectE || pair[0].seq != expectQ {
		return unknown, pair, errors.New("维护SQL版本不符合永久阶段")
	}
	if next == unknown || next <= phase {
		return phase, pair, tx.Rollback()
	}
	if next != phase+1 {
		return unknown, pair, errors.New("维护阶段不可跳跃")
	}
	if next == databasepb.SaveBarrierMaintenanceReceipt_SQL_PROMOTED {
		for _, name := range []string{"player_database", "player_database_1"} {
			result, err := tx.ExecContext(ctx, "UPDATE `"+p.RowSchema+"`."+name+" SET fence_owner_epoch=?,fence_save_seq=? WHERE player_id=? AND fence_owner_epoch=? AND fence_save_seq=?", p.TargetOwner, p.TargetSaveSeq, p.PlayerId, p.OriginalSaveEpoch, p.OriginalSaveSeq)
			if err != nil {
				return unknown, pair, err
			}
			n, err := result.RowsAffected()
			if err != nil || n != 1 {
				return unknown, pair, errors.New("双表围栏提升CAS失败")
			}
		}
		updated, err := readPair(ctx, tx, p.RowSchema, row.ID)
		if err != nil {
			return unknown, pair, err
		}
		for i := range updated {
			if updated[i].digest != pair[i].digest || updated[i].schemaGen != pair[i].schemaGen || updated[i].epoch != p.TargetOwner || updated[i].seq != p.TargetSaveSeq {
				return unknown, pair, errors.New("提升改变业务bytes或世代")
			}
		}
	}
	result, err := tx.ExecContext(ctx, "UPDATE "+table+" SET phase=?,updated_ms=CAST(UNIX_TIMESTAMP(NOW(3))*1000 AS UNSIGNED) WHERE receipt_id=? AND phase=?", next, p.ReceiptId, phase)
	if err != nil {
		return unknown, pair, err
	}
	n, err := result.RowsAffected()
	if err != nil || n != 1 {
		return unknown, pair, errors.New("维护阶段CAS失败")
	}
	if err := tx.Commit(); err != nil {
		return unknown, pair, fmt.Errorf("维护阶段提交结局未知，仅原计划重读: %w", err)
	}
	return next, pair, nil
}

// 复用既有debug_import本地标记合同；db模块不反向依赖DS工具包。两个schema同实例nonce，
// 每个实际SQL事务持标记行锁复核，不能只拿事务外地址/强TiDB探针当写目标资格。
func (r Restorer) checkPromotionSQLTarget(ctx context.Context, tx *sql.Tx, p Plan) error {
	if len(r.LocalTargetID) != 32 || strings.Trim(r.LocalTargetID, "0123456789abcdef") != "" {
		return errors.New("维护SQL实例nonce缺失")
	}
	for _, schema := range []string{p.RouteSchema, p.Schema} {
		for _, want := range []string{"local-debug-import-target", "old-scene-recovery:" + r.LocalTargetID} {
			var got string
			if err := tx.QueryRowContext(ctx, "SELECT marker FROM `"+schema+"`.debug_import_local_target WHERE marker=? FOR UPDATE", want).Scan(&got); err != nil || got != want {
				return errors.New("维护目标缺已审本地实例标记")
			}
		}
	}
	return nil
}

// 首版只恢复 home zone 自己的 zone 库。异区钉落点/player_store 必须另验权威 placement，不能猜。
func validTarget(schema string, zone uint32, addr string, db int) bool {
	host, port, err := net.SplitHostPort(addr)
	p, pErr := strconv.ParseUint(port, 10, 16)
	return zone > 0 && zone <= 999999 && schema == fmt.Sprintf("zone_%d_db", zone) &&
		err == nil && pErr == nil && p > 0 && host != "" && !strings.ContainsAny(host, " /\\@\r\n\t") && db == 0
}

func (p Plan) Validate() error {
	if p.ValidateTarget() != nil ||
		len(p.MaintenanceID) == 0 || len(p.MaintenanceID) > 128 || len(p.Players) == 0 || len(p.Players) > 1000 {
		return errors.New("维护计划格式、范围或工单号无效")
	}
	seen := make(map[string]bool, len(p.Players))
	var audited uint64
	if p.Format == FormatV2 {
		var err error
		audited, err = auditedBound(p.AuditedOwnerUpperBound)
		if err != nil {
			return err
		}
	} else if p.AuditedOwnerUpperBound != "" {
		return errors.New("v1 不接受新增恢复证明")
	}
	for _, row := range p.Players {
		if _, err := positive(row.ID); err != nil || seen[row.ID] {
			return errors.New("维护计划玩家号无效或重复")
		}
		seen[row.ID] = true
		if err := validatePlannedRoute(p, row); err != nil {
			return err
		}
		owner, err := positive(row.Owner)
		if err != nil {
			return errors.New("维护计划归属计数器无效")
		}
		if p.Format == FormatV2 && owner <= audited {
			return errors.New("当前 owner 未严格高于独立审核的旧代上界")
		}
		e, s, err := savebarrier.Parse(row.Original)
		if err != nil || (e == 0) != (s == 0) || e > owner {
			return errors.New("维护计划原始围栏无效")
		}
		want := row.Original
		if e == 0 {
			if !p.AllowLegacy {
				return errors.New("零围栏需要显式允许历史迁移")
			}
			want = savebarrier.Version(owner, 1)
		}
		if row.Target != want {
			return errors.New("维护计划目标围栏无效")
		}
		for _, digest := range row.Rows {
			b, err := hex.DecodeString(digest)
			if err != nil || len(b) != sha256.Size || hex.EncodeToString(b) != digest {
				return errors.New("维护计划 SQL 行摘要无效")
			}
		}
	}
	return nil
}

func positive(raw string) (uint64, error) {
	v, err := strconv.ParseUint(raw, 10, 64)
	if err != nil || v == 0 || strconv.FormatUint(v, 10) != raw {
		return 0, errors.New("非规范正整数")
	}
	return v, nil
}

// Prepare 在两行点锁下生成计划，只读业务数据；不写 SQL、Redis 或资产明文。
func (r Restorer) Prepare(ctx context.Context, schema string, zone uint32, maintenanceID string, ids []string, allowLegacy bool) (Plan, error) {
	p := Plan{Format: Format, Schema: schema, Zone: zone, MaintenanceID: maintenanceID, AllowLegacy: allowLegacy, RedisAddr: r.Redis.Options().Addr, RedisDB: r.Redis.Options().DB, RoutingMode: r.RoutingMode, LocalMySQL: r.LocalMySQL}
	p.AllowHomeFallback = r.AllowHomeFallback
	if r.MappingRedis != nil || r.RouteSchema != "" || r.RoutingMode != RoutingModeRedis || schema != fmt.Sprintf("zone_%d_db", zone) || p.RedisDB != 0 || r.AuditedOwnerUpperBound != "" {
		p.Format, p.RouteSchema = FormatV2, r.RouteSchema
		p.AuditedOwnerUpperBound = r.AuditedOwnerUpperBound
		p.MappingAddr, p.MappingDB = r.mapping().Options().Addr, r.mapping().Options().DB
	}
	// 先校验范围，避免非法输入触发连接或读取。
	if p.ValidateTarget() != nil || len(ids) == 0 || len(ids) > 1000 || len(maintenanceID) == 0 || len(maintenanceID) > 128 {
		return p, errors.New("维护范围无效")
	}
	seen := map[string]bool{}
	if p.Format == FormatV2 {
		if _, err := auditedBound(p.AuditedOwnerUpperBound); err != nil {
			return p, err
		}
	}
	for _, id := range ids {
		if _, err := positive(id); err != nil || seen[id] {
			return p, errors.New("玩家号无效或重复")
		}
		seen[id] = true
	}
	for _, id := range ids {
		row, err := r.prepareOne(ctx, p, id)
		if err != nil {
			return p, fmt.Errorf("玩家 %s 预检失败: %w", id, err)
		}
		p.Players = append(p.Players, row)
	}
	return p, p.Validate()
}

func (r Restorer) prepareOne(ctx context.Context, p Plan, id string) (Player, error) {
	ctx, cancel := context.WithTimeout(ctx, 5*time.Second)
	defer cancel()
	tx, err := r.DB.BeginTx(ctx, &sql.TxOptions{Isolation: sql.LevelReadCommitted})
	if err != nil {
		return Player{}, err
	}
	defer tx.Rollback()
	row, err := r.lockRoute(ctx, tx, p, id)
	if err != nil {
		return Player{}, err
	}
	rows, err := readPair(ctx, tx, p.Schema, id)
	if err != nil {
		return Player{}, err
	}
	pid, _ := positive(id)
	// GET 仅获取计数器；Lua 在同一原子段复核它，不能把此预检当成发布点。
	owner, err := r.Redis.Get(ctx, ownerepoch.OwnerEpochKey(pid)).Result()
	if err != nil {
		return Player{}, errors.New("归属计数器缺失或无法读取，需要先单独恢复计数器")
	}
	n, err := positive(owner)
	if err != nil || n < rows[0].epoch {
		return Player{}, errors.New("归属计数器缺失、畸形或落后于 SQL")
	}
	row.Owner, row.Original, row.Rows = owner, savebarrier.Version(rows[0].epoch, rows[0].seq), [2]string{rows[0].digest, rows[1].digest}
	row.Target = row.Original
	if rows[0].epoch == 0 {
		if !p.AllowLegacy {
			return Player{}, errors.New("历史零围栏未获迁移选项")
		}
		row.Target = savebarrier.Version(n, 1)
	}
	if err := r.inspect(ctx, p, row, false, row.Original == row.Target, restoredSchemaGen(rows)); err != nil {
		return Player{}, err
	}
	return row, nil
}

// ApplyOne 每次只提交一名计划内玩家。失败保留维护标记，已成功的前缀可按原计划重入。
// 零围栏先原子提交双表元数据，再重新点锁强读复核，锁内才发布 Redis；SQL 提交失败绝不发布 ACK。
func (r Restorer) ApplyOne(ctx context.Context, p Plan, index int) error {
	if err := p.Validate(); err != nil {
		return err
	}
	if p.RedisAddr != r.Redis.Options().Addr || p.RedisDB != r.Redis.Options().DB || p.RoutingMode != r.RoutingMode || p.LocalMySQL != r.LocalMySQL || p.RouteSchema != r.RouteSchema || (p.Format == FormatV2 && (p.MappingAddr != r.mapping().Options().Addr || p.MappingDB != r.mapping().Options().DB)) {
		return errors.New("实际 Redis 连接或路由模式偏离审定计划")
	}
	if p.Format == Format && (r.mapping().Options().Addr != p.RedisAddr || r.mapping().Options().DB != 0) {
		return errors.New("v1 实际映射必须与父缓存同实例 DB0")
	}
	if p.AuditedOwnerUpperBound != r.AuditedOwnerUpperBound {
		return errors.New("独立审核的旧代上界偏离计划")
	}
	if index < 0 || index >= len(p.Players) {
		return errors.New("维护计划玩家索引越界")
	}
	ctx, cancel := context.WithTimeout(ctx, 5*time.Second)
	defer cancel()
	row := p.Players[index]
	tx, err := r.DB.BeginTx(ctx, &sql.TxOptions{Isolation: sql.LevelReadCommitted})
	if err != nil {
		return err
	}
	defer func() {
		if tx != nil {
			_ = tx.Rollback()
		}
	}()
	if err := r.recheckRoute(ctx, tx, p, row); err != nil {
		return err
	}
	current, err := readPair(ctx, tx, p.Schema, row.ID)
	if err != nil {
		return err
	}
	if err := match(row, current); err != nil {
		return err
	}
	if err := r.inspect(ctx, p, row, false, savebarrier.Version(current[0].epoch, current[0].seq) == row.Target, restoredSchemaGen(current)); err != nil {
		return err
	}
	if savebarrier.Version(current[0].epoch, current[0].seq) != row.Target {
		e, s, _ := savebarrier.Parse(row.Target)
		for _, table := range []string{"player_database", "player_database_1"} {
			result, err := tx.ExecContext(ctx, "UPDATE `"+p.Schema+"`."+table+" SET fence_owner_epoch=?,fence_save_seq=? WHERE player_id=? AND fence_owner_epoch=0 AND fence_save_seq=0", e, s, row.ID)
			if err != nil {
				return err
			}
			n, err := result.RowsAffected()
			if err != nil || n != 1 {
				return errors.New("历史围栏迁移 CAS 失败")
			}
		}
		if err := tx.Commit(); err != nil {
			return fmt.Errorf("双表围栏提交失败，未发布屏障: %w", err)
		}
		tx, err = r.DB.BeginTx(ctx, &sql.TxOptions{Isolation: sql.LevelReadCommitted})
		if err != nil {
			return err
		}
		if err := r.recheckRoute(ctx, tx, p, row); err != nil {
			return err
		}
		current, err = readPair(ctx, tx, p.Schema, row.ID)
		if err != nil {
			return err
		}
		if err := match(row, current); err != nil {
			return err
		}
		if savebarrier.Version(current[0].epoch, current[0].seq) != row.Target {
			return errors.New("已提交围栏在重锁期间改变")
		}
	}
	if err := r.inspect(ctx, p, row, true, true, restoredSchemaGen(current)); err != nil {
		return err
	}
	// 本事务只有锁定读。Redis 已确认真实提交的 SQL，结束读事务不改变资产结局。
	return tx.Rollback()
}

type sqlRow struct {
	epoch, seq uint64
	schemaGen  uint32
	digest     string
	values     map[string][]byte
}

// 恢复以 SQL 为准，屏障 hash 的 schema_gen 写成恢复之后玩家实际会加载到的数据的世代：同一事务已点锁两行的
// 最小戳，未盖戳 0 按 1。与 scene C3、login C4 的"加载世代 = 两表戳最小值"同一口径
// (docs/design/mixed-version-persist-field-protection.md §3.3、§13 第 8 条)：取最大值的话，两行戳不同(单表进过死信)
// 时冷加载出的数据加载世代低于 hash，恢复后的第一次进游戏与存盘都会被判倒退，enforce 下这名玩家既进不来也存不了。
// 世代仍来自 SQL 原件且纳入原全列摘要；它不进入可编辑计划，也不修改 SQL 列。
func restoredSchemaGen(rows [2]sqlRow) uint32 {
	gen := uint32(0)
	for _, row := range rows {
		effective := row.schemaGen
		if effective == 0 {
			effective = 1
		}
		if gen == 0 || effective < gen {
			gen = effective
		}
	}
	return gen
}

func readPair(ctx context.Context, tx *sql.Tx, schema, id string) ([2]sqlRow, error) {
	var pair [2]sqlRow
	for i, table := range []string{"player_database", "player_database_1"} {
		row, err := readRow(ctx, tx, schema, table, id)
		if err != nil {
			return pair, err
		}
		pair[i] = row
	}
	if pair[0].epoch != pair[1].epoch || pair[0].seq != pair[1].seq || (pair[0].epoch == 0) != (pair[0].seq == 0) {
		return pair, errors.New("两张 SQL 行围栏不同版或不完整")
	}
	return pair, nil
}

func readRow(ctx context.Context, tx *sql.Tx, schema, table, id string) (sqlRow, error) {
	rows, err := tx.QueryContext(ctx, "SELECT * FROM `"+schema+"`."+table+" WHERE player_id=? FOR UPDATE", id)
	if err != nil {
		return sqlRow{}, err
	}
	defer rows.Close()
	columns, err := rows.Columns()
	if err != nil {
		return sqlRow{}, err
	}
	if !rows.Next() {
		if err := rows.Err(); err != nil {
			return sqlRow{}, err
		}
		return sqlRow{}, errors.New("SQL 玩家行缺失")
	}
	values := make([]sql.RawBytes, len(columns))
	args := make([]any, len(columns))
	for i := range values {
		args[i] = &values[i]
	}
	if err := rows.Scan(args...); err != nil {
		return sqlRow{}, err
	}
	var row sqlRow
	row.values = make(map[string][]byte, len(columns))
	if len(columns) == 0 || len(columns) > 256 {
		return row, errors.New("SQL玩家列预算无效")
	}
	found := [4]bool{}
	h := sha256.New()
	totalBytes := 0
	for i, col := range columns {
		v := values[i]
		totalBytes += len(v)
		if totalBytes > 16<<20 {
			return row, errors.New("SQL玩家原件单行超过16MiB预算")
		}
		if _, duplicate := row.values[col]; duplicate {
			return row, errors.New("SQL玩家列重复")
		}
		row.values[col] = bytes.Clone(v)
		switch col {
		case "fence_owner_epoch", "fence_save_seq":
			n, err := strconv.ParseUint(string(v), 10, 64)
			if err != nil || strconv.FormatUint(n, 10) != string(v) {
				return row, errors.New("SQL 围栏值无效")
			}
			if col == "fence_owner_epoch" {
				row.epoch, found[1] = n, true
			} else {
				row.seq, found[2] = n, true
			}
			continue
		case "player_id":
			if string(v) != id {
				return row, errors.New("SQL 行玩家身份不符")
			}
			found[0] = true
		case "persist_schema_gen":
			// 正常迁移先补NOT NULL DEFAULT 0列；缺列、NULL与坏值不能静默当历史0。
			n, err := strconv.ParseUint(string(v), 10, 32)
			if err != nil || strconv.FormatUint(n, 10) != string(v) {
				return row, errors.New("SQL 持久化世代不是规范 uint32")
			}
			row.schemaGen, found[3] = uint32(n), true
			// 不continue：原始戳字节与所有其它非围栏列一起进入原计划摘要。
		}
		// 长度前缀与 NULL 标记避免字段边界或 NULL/空串出现相同摘要。
		var size [8]byte
		binary.BigEndian.PutUint64(size[:], uint64(len(col)))
		h.Write(size[:])
		h.Write([]byte(col))
		if v == nil {
			h.Write([]byte{0})
		} else {
			h.Write([]byte{1})
			binary.BigEndian.PutUint64(size[:], uint64(len(v)))
			h.Write(size[:])
			h.Write(v)
		}
	}
	if found != [4]bool{true, true, true, true} {
		return row, errors.New("SQL 玩家身份、围栏或持久化世代列缺失，须先完成正常迁移")
	}
	if rows.Next() {
		return row, errors.New("SQL 玩家主键不唯一")
	}
	if err := rows.Err(); err != nil {
		return row, err
	}
	row.digest = hex.EncodeToString(h.Sum(nil))
	return row, nil
}

func match(want Player, current [2]sqlRow) error {
	v := savebarrier.Version(current[0].epoch, current[0].seq)
	if v != want.Original && v != want.Target {
		return errors.New("SQL 围栏偏离审定计划")
	}
	for i := range current {
		if current[i].digest != want.Rows[i] {
			return errors.New("SQL 资产行偏离审定计划")
		}
	}
	return nil
}

func validRoute(zone uint32, home, raw string, allowHomeFallback bool) error {
	homeZone, present, err := placement.ParseHomeZone(home)
	if err != nil || !present || homeZone != zone || home != strconv.FormatUint(uint64(zone), 10) {
		return errors.New("玩家 home zone 与计划不符")
	}
	if homeZoneFallback(allowHomeFallback, raw) {
		// v1 的 schema 已由 validTarget 钉为 zone_<home>_db,正是 go/db 给无记录玩家选的库;发布时 Lua 再原子复核记录仍不存在。
		return nil
	}
	rec, err := placement.Parse(raw)
	if err != nil || rec.Frozen || rec.StorageID != zone || rec.Encode() != raw {
		return errors.New("玩家不在本区稳定落点，须走有权威路由证明的维护流程")
	}
	return nil
}

// homeZoneFallback 报告这名玩家是否按「无落点记录 → home 的 zone 库」恢复(2026-10-20 修复，r2-mz#1)。
//
// 只在计划显式允许且落点原值为空时成立；调用方另行保证 home == 计划 zone、schema 的库号 == home。放宽的边界：
//   - 与 go/db 选库同一规则(placement.EffectiveStorage：记录 ?? home)，恢复出的屏障指向的正是运行期冷加载要读的两行；
//     有记录的玩家(钉落点、搬库、pin 合服)仍走原来的稳定落点校验，不受影响。
//   - 屏障只按当前 SQL 两行的围栏重建，不会把任何旧屏障恢复回来：已有屏障仍须与目标版本逐字段相同，否则照旧拒绝。
//   - 现有原子校验一条不少：永久维护标记、owner、home 原值、无合服围栏、缓存 / 证据 / 交接标记不存在、64 桶为空；
//     同实例时落点记录「仍不存在」在写屏障的同一段 Lua 里复核(scripts.go),拆实例时由 mapping 维护检查前后各核一次。
func homeZoneFallback(allow bool, rawPlacement string) bool {
	return allow && rawPlacement == ""
}

func MaintenanceKey(zone uint32) string {
	return fmt.Sprintf("player:save_barrier:maintenance:%d", zone)
}

func recoveryKeys(zone uint32, id uint64) []string {
	first := string((&databasepb.PlayerDatabase{}).ProtoReflect().Descriptor().FullName())
	second := string((&databasepb.PlayerDatabase_1{}).ProtoReflect().Descriptor().FullName())
	keys := []string{MaintenanceKey(zone), ownerepoch.OwnerEpochKey(id), savebarrier.ParentKey(id), fmt.Sprintf("%s:%d", first, id), fmt.Sprintf("%s:%d", second, id), fmt.Sprintf("player:{%d}:asset_sql_commit", id), ownerepoch.HandoffKey(id), savebarrier.Key(id)}
	for bucket := 0; bucket < 64; bucket++ {
		keys = append(keys, fmt.Sprintf("player:save_pending:%d:%d", zone, bucket))
	}
	keys = append(keys, placement.HomeZoneKey(id), placement.Key(id), placement.MergeFenceKey(zone))
	return keys
}

func (r Restorer) inspect(ctx context.Context, p Plan, row Player, publish, committed bool, schemaGen uint32) error {
	id, _ := positive(row.ID)
	write := "0"
	if publish {
		write = "1"
	}
	allowExisting := "0"
	if committed {
		allowExisting = "1"
	}
	keys := recoveryKeys(p.Zone, id)
	if p.RoutingMode != RoutingModeRedis {
		if err := r.inspectMapping(ctx, p, row); err != nil {
			return err
		}
		keys = keys[:72]
	}
	// 首次读取的原件只作为本次 Lua CAS 输入；不能缺省空字段冒缺键。
	prior, err := r.Redis.HGet(ctx, savebarrier.Key(id), "handoff_fence").Result()
	if errors.Is(err, redis.Nil) {
		prior = ""
	} else if err != nil {
		return fmt.Errorf("读取交接资格下限: %w", err)
	} else if prior == "" {
		return errors.New("交接资格下限为空值")
	}
	var token [16]byte
	if _, err := rand.Read(token[:]); err != nil {
		return fmt.Errorf("轮换交接撤销令牌: %w", err)
	}
	n, err := restoreScript.Run(ctx, r.Redis, keys, p.MaintenanceID, row.Owner, row.Target, write, allowExisting, row.HomeZone, row.Placement, strconv.FormatUint(uint64(schemaGen), 10), prior, hex.EncodeToString(token[:])).Int()
	if err != nil {
		return fmt.Errorf("维护屏障检查拒绝: %w", err)
	}
	if n != 1 {
		return errors.New("维护屏障回复无效")
	}
	if p.RoutingMode != RoutingModeRedis {
		return r.inspectMapping(ctx, p, row)
	}
	return nil
}

// MaintenanceReleasePlan 是显式维护动作的封套，不取代任何业务 proto。
// 成员及外部审定原件固定；每次执行的短排他窗独立输入，不能改原计划续期。
type MaintenanceReleasePlan struct {
	Format        string                     `json:"format"`
	Intent        string                     `json:"intent_id"`
	Zone          uint32                     `json:"zone"`
	MaintenanceID string                     `json:"maintenance_id"`
	LocalTarget   string                     `json:"local_target_id"`
	RouteSchema   string                     `json:"route_schema"`
	Review        ReleaseFile                `json:"review"`
	Members       []MaintenanceReleaseMember `json:"members"`
	Targets       []MaintenanceReleaseTarget `json:"targets"`
	SQLSHA256     string                     `json:"sql_snapshot_sha256"`
}
type MaintenanceReleaseMember struct {
	PlayerID       uint64      `json:"player_id"`
	RowSchema      string      `json:"row_schema"`
	PromotionProto []byte      `json:"promotion_proto,omitempty"`
	RecoveryProtos [][]byte    `json:"recovery_protos,omitempty"`
	CancelAction   ReleaseFile `json:"cancel_action"`
	// 来自A原全列观察；普通未恢复成员也须钉业务摘要/原版本，不能只从当前SQL重新取基线。
	FirstBusinessSHA256  string `json:"first_business_sha256"`
	SecondBusinessSHA256 string `json:"second_business_sha256"`
	Owner                uint64 `json:"owner_epoch"`
	SaveSeq              uint64 `json:"save_seq"`
	Fence                string `json:"handoff_fence"`
}
type MaintenanceReleaseTarget struct {
	Role          string                    `json:"role"`
	Addr          string                    `json:"addr"`
	DB            int                       `json:"db"`
	InstanceNonce string                    `json:"instance_nonce"`
	Guards        []MaintenanceReleaseGuard `json:"guards"`
}
type MaintenanceReleaseGuard struct {
	Key   string `json:"key"`
	Value string `json:"value"`
}
type ReleaseFile struct {
	Path   string `json:"path"`
	SHA256 string `json:"sha256"`
}

const maintenanceReleaseFormat = "player-save-explicit-maintenance-release-v1"

// PrepareMaintenanceRelease 只读SQL/Redis，不写收据、guard或业务状态。
// permit须由同一审定review和本次新排他窗构造；SQL不宣称能保证进程/操作者排他。
func (r Restorer) PrepareMaintenanceRelease(ctx context.Context, zone uint32, maintenance string, members []MaintenanceReleaseMember, permit *MaintenanceReleasePermit) (MaintenanceReleasePlan, error) {
	p := MaintenanceReleasePlan{Format: maintenanceReleaseFormat, Zone: zone, MaintenanceID: maintenance, LocalTarget: r.LocalTargetID, RouteSchema: r.RouteSchema, Members: members}
	if permit == nil || permit.planSHA != "" {
		return p, errors.New("只读release-plan需要未绑定计划的新审定窗口")
	}
	p.Review = permit.review
	nonce := make([]byte, 32)
	if _, err := rand.Read(nonce); err != nil {
		return p, err
	}
	p.Intent = hex.EncodeToString(nonce)
	var err error
	p.Targets, err = r.releaseTargets(ctx, zone, maintenance)
	if err != nil {
		return p, err
	}
	if err = r.validateMaintenanceRelease(p); err != nil {
		return p, err
	}
	if err = permit.check(p, ""); err != nil {
		return p, err
	}
	p.SQLSHA256, err = r.releaseSQLSnapshot(ctx, p)
	if err != nil {
		return p, err
	}
	// 计划创建只准全部原guard在、收据缺席；已提交前缀必须用原计划，不重造intent。
	for _, target := range p.Targets {
		state, err := r.releaseTarget(ctx, p, target, "", false, permit.expiresMS)
		if err != nil || state != "pending" {
			return p, errors.New("release-plan发现已提交/损坏目标，必须保原计划消歧")
		}
	}
	return p, permit.check(p, "")
}

// ApplyMaintenanceRelease 先复核全部物理目标，再逐目标原子提交。未知应答不自动重发。
// 同原计划重入从入口即接受精确committed前缀；仍不unfreeze SQL或重启业务。
func (r Restorer) ApplyMaintenanceRelease(ctx context.Context, p MaintenanceReleasePlan, planSHA string, permit *MaintenanceReleasePermit) error {
	if err := r.validateMaintenanceRelease(p); err != nil {
		return err
	}
	if !releaseHex(planSHA, 64) || permit == nil {
		return errors.New("必须原计划全文件SHA和新的短排他窗")
	}
	original, err := json.MarshalIndent(p, "", "  ")
	if err != nil {
		return err
	}
	original = append(original, '\n')
	fileSum := sha256.Sum256(original)
	if hex.EncodeToString(fileSum[:]) != planSHA {
		return errors.New("typed计划与原CLI规范整文件SHA不符，禁止换intent/正文")
	}
	if err := permit.check(p, planSHA); err != nil {
		return err
	}
	snapshot, err := r.releaseSQLSnapshot(ctx, p)
	if err != nil || snapshot != p.SQLSHA256 {
		return errors.New("解除前SQL全列/成员/永久原件发生变化")
	}
	for _, target := range p.Targets {
		if _, err = r.releaseTarget(ctx, p, target, planSHA, false, permit.expiresMS); err != nil {
			return err
		}
	}
	for _, target := range p.Targets {
		if err = permit.check(p, planSHA); err != nil {
			return err
		}
		// 网络I/O均发生在SQL事务之外。SQL永久冻结和外部禁止新writer是显式前置。
		if _, err = r.releaseTarget(ctx, p, target, planSHA, true, permit.expiresMS); err != nil {
			return fmt.Errorf("解除结果未知/失败，保原计划及已提交前缀，SQL仍冻结: %w", err)
		}
	}
	for _, target := range p.Targets {
		state, e := r.releaseTarget(ctx, p, target, planSHA, false, permit.expiresMS)
		if e != nil || state != "committed" {
			return errors.New("解除尾核未确认完整永久收据")
		}
	}
	snapshot, err = r.releaseSQLSnapshot(ctx, p)
	if err != nil || snapshot != p.SQLSHA256 {
		return errors.New("解除后SQL全列改变；禁止unfreeze/重启，保全部原件")
	}
	return permit.check(p, planSHA)
}

func releaseHex(v string, n int) bool {
	return len(v) == n && strings.Trim(v, "0123456789abcdef") == "" && v != strings.Repeat("0", n)
}
func maintenanceReleaseReceiptKey(zone uint32, maintenance string) string {
	h := sha256.Sum256([]byte(maintenance))
	return fmt.Sprintf("player:save_barrier:maintenance_release:%d:%x", zone, h)
}
func (r Restorer) releaseTargets(ctx context.Context, zone uint32, maintenance string) ([]MaintenanceReleaseTarget, error) {
	if r.Redis == nil || r.mapping() == nil {
		return nil, errors.New("缺两个维护专用Redis连接")
	}
	targets := []MaintenanceReleaseTarget{}
	for _, item := range []struct {
		role   string
		client *redis.Client
		guards []MaintenanceReleaseGuard
	}{
		{"core", r.Redis, []MaintenanceReleaseGuard{{MaintenanceKey(zone), maintenance}}},
		{"mapping", r.mapping(), []MaintenanceReleaseGuard{{MaintenanceKey(zone), maintenance}, {placement.MergeFenceKey(zone), routeGuardValue(maintenance)}}},
	} {
		c := item.client
		nonce, err := c.Get(ctx, "debug_import:local_instance_id").Result()
		if err != nil || !promotionTargetID(nonce) {
			return nil, errors.New("缺永久Redis实例nonce")
		}
		merged := -1
		for i, t := range targets {
			if t.Addr == c.Options().Addr && t.DB == c.Options().DB {
				merged = i
			}
		}
		if merged >= 0 {
			t := &targets[merged]
			if t.InstanceNonce != nonce {
				return nil, errors.New("同一物理目标nonce观察冲突")
			}
			for _, g := range item.guards {
				found := false
				for _, prior := range t.Guards {
					if prior.Key == g.Key {
						if prior.Value != g.Value {
							return nil, errors.New("同key不同原guard")
						}
						found = true
					}
				}
				if !found {
					t.Guards = append(t.Guards, g)
				}
			}
			continue
		}
		targets = append(targets, MaintenanceReleaseTarget{Role: item.role, Addr: c.Options().Addr, DB: c.Options().DB, InstanceNonce: nonce, Guards: item.guards})
	}
	return targets, nil
}
func (r Restorer) validateMaintenanceRelease(p MaintenanceReleasePlan) error {
	if p.Format != maintenanceReleaseFormat || !releaseHex(p.Intent, 64) || p.Zone == 0 || p.Zone > placement.MaxZoneStorageID || p.MaintenanceID == "" || len(p.MaintenanceID) > 128 || p.LocalTarget != r.LocalTargetID || !promotionTargetID(p.LocalTarget) || p.RouteSchema != r.RouteSchema || !validSchema(p.RouteSchema) || r.RoutingMode != RoutingModeDBFirst || r.LocalMySQL || r.DB == nil || r.Redis == nil || r.mapping() == nil || len(p.Members) == 0 || len(p.Members) > 1000 {
		return errors.New("显式解除仅限原TiDB/db_first/本地实例/完整具名成员")
	}
	seen := map[uint64]bool{}
	recovered := 0
	for _, m := range p.Members {
		if m.PlayerID == 0 || seen[m.PlayerID] || !validSchema(m.RowSchema) || m.Owner == 0 || m.SaveSeq == 0 || !releaseHex(m.FirstBusinessSHA256, 64) || !releaseHex(m.SecondBusinessSHA256, 64) {
			return errors.New("解除成员重复/非法/缺A原资产业务摘要与版本")
		}
		seen[m.PlayerID] = true
		if len(m.PromotionProto) == 0 {
			if len(m.RecoveryProtos) != 0 || m.CancelAction.Path != "" {
				return errors.New("无promotion成员不能伪带领域恢复原件")
			}
			continue
		}
		recovered++
		source := &databasepb.SaveBarrierMaintenanceReceipt_PromotionPlan{}
		if proto.Unmarshal(m.PromotionProto, source) != nil || ValidatePromotion(source) != nil || source.PlayerId != m.PlayerID || source.RowSchema != m.RowSchema || source.RouteSchema != p.RouteSchema || source.HomeZone != p.Zone || source.MaintenanceId != p.MaintenanceID || len(m.RecoveryProtos) == 0 {
			return errors.New("promotion不属于原维护/成员，或缺永久退休原件")
		}
		raw, _, err := PromotionDigest(source)
		if err != nil || !bytes.Equal(raw, m.PromotionProto) {
			return errors.New("promotion不是原规范PB bytes")
		}
		if m.Owner != source.TargetOwner || m.SaveSeq != source.TargetSaveSeq || m.Fence != source.RevokedFence || m.FirstBusinessSHA256 != hex.EncodeToString(source.FirstBusinessSha256) || m.SecondBusinessSHA256 != hex.EncodeToString(source.SecondBusinessSha256) {
			return errors.New("恢复成员业务/双版本/永久R不等原promotion")
		}
	}
	if recovered == 0 {
		return errors.New("显式恢复解除至少需要本维护的真实永久恢复原件")
	}
	// 物理目标/guard集合只由固定core/mapping连接派生，不接受任意key。
	if len(p.Targets) < 1 || len(p.Targets) > 2 {
		return errors.New("解除物理目标数非法")
	}
	if p.Targets[0].Addr != r.Redis.Options().Addr || p.Targets[0].DB != r.Redis.Options().DB {
		return errors.New("提交顺序必须固定core先，不能重排物理前缀")
	}

	expected := map[string]map[string]string{}
	// go-redis把构造参数MaxRetries:-1规范为Options().MaxRetries==0；默认构造会规范为3。
	for _, c := range []*redis.Client{r.Redis, r.mapping()} {
		if c == nil || c.Options().MaxRetries != 0 {
			return errors.New("解除连接必须关闭自动命令重试")
		}
		key := fmt.Sprintf("%s/%d", c.Options().Addr, c.Options().DB)
		if expected[key] == nil {
			expected[key] = map[string]string{MaintenanceKey(p.Zone): p.MaintenanceID}
		}
	}
	mk := fmt.Sprintf("%s/%d", r.mapping().Options().Addr, r.mapping().Options().DB)
	expected[mk][placement.MergeFenceKey(p.Zone)] = routeGuardValue(p.MaintenanceID)
	for _, t := range p.Targets {
		key := fmt.Sprintf("%s/%d", t.Addr, t.DB)
		guards, ok := expected[key]
		if !ok || !promotionTargetID(t.InstanceNonce) || len(guards) != len(t.Guards) {
			return errors.New("目标/guard不是固定全集")
		}
		for _, g := range t.Guards {
			if guards[g.Key] != g.Value {
				return errors.New("guard不匹配")
			}
			delete(guards, g.Key)
		}
		if len(guards) != 0 {
			return errors.New("guard重复/遗漏")
		}
		delete(expected, key)
	}
	if len(expected) != 0 {
		return errors.New("物理目标遗漏")
	}
	return nil
}
func (r Restorer) releaseTarget(ctx context.Context, p MaintenanceReleasePlan, t MaintenanceReleaseTarget, planSHA string, apply bool, expiresMS int64) (string, error) {
	var client *redis.Client
	for _, c := range []*redis.Client{r.Redis, r.mapping()} {
		if c != nil && c.Options().Addr == t.Addr && c.Options().DB == t.DB {
			client = c
			break
		}
	}
	if client == nil {
		return "", errors.New("目标连接已变")
	}
	mode := "check"
	if apply {
		mode = "apply"
	}
	body, _ := json.Marshal(p)
	inner := sha256.Sum256(body)
	guardRaw, _ := json.Marshal(t.Guards)
	gd := sha256.Sum256(guardRaw)
	receipt := ""
	if planSHA != "" {
		receipt = fmt.Sprintf("release-v1|%s|%s|%x|%s|%x", p.Intent, planSHA, inner, t.InstanceNonce, gd)
	}
	keys := []string{"debug_import:local_target", "debug_import:local_instance_id", maintenanceReleaseReceiptKey(p.Zone, p.MaintenanceID)}
	args := []any{mode, "local-debug-import-target", t.InstanceNonce, receipt, strconv.Itoa(len(t.Guards)), "0", strconv.FormatInt(expiresMS, 10)}
	for _, g := range t.Guards {
		keys = append(keys, g.Key)
		args = append(args, g.Value)
	}
	if t.Addr == r.Redis.Options().Addr && t.DB == r.Redis.Options().DB {
		args[5] = strconv.Itoa(len(p.Members))
		for _, m := range p.Members {
			keys = append(keys, ownerepoch.OwnerEpochKey(m.PlayerID), savebarrier.Key(m.PlayerID))
			args = append(args, strconv.FormatUint(m.Owner, 10), savebarrier.Version(m.Owner, m.SaveSeq), m.Fence)
		}
	}
	// EVAL一次，不用Script.Run的NOSCRIPT隐式重发路径；MaxRetries=-1也在入口强制。
	return client.Eval(ctx, maintenanceReleaseLua, keys, args...).Text()
}

// releaseSQLSnapshot只读一致事务；全部列含NULL/blob入摘要。无事务内Redis/Kafka/Scene调用。
// 验证的是正式领域终态和永久原件，不重新实现Cancel的业务裁决或更新任何P/state。
func (r Restorer) releaseSQLSnapshot(ctx context.Context, p MaintenanceReleasePlan) (string, error) {
	tx, err := r.DB.BeginTx(ctx, &sql.TxOptions{Isolation: sql.LevelRepeatableRead, ReadOnly: true})
	if err != nil {
		return "", err
	}
	defer tx.Rollback()
	evidence := []releaseSQLRecord{}
	read := func(label, q string, args ...any) ([]releaseSQLRecord, error) {
		rows, e := readMaintenanceReleaseRows(ctx, tx, label, q, args...)
		if e == nil {
			evidence = append(evidence, rows...)
		}
		return rows, e
	}
	for _, schema := range append([]string{p.RouteSchema}, releaseMemberSchemas(p.Members)...) {
		rows, e := read("marker/"+schema, "SELECT marker FROM `"+schema+"`.debug_import_local_target WHERE marker IN (?,?) ORDER BY marker", "local-debug-import-target", "old-scene-recovery:"+p.LocalTarget)
		if e != nil || len(rows) != 2 || string(rows[0].value("marker")) != "local-debug-import-target" || string(rows[1].value("marker")) != "old-scene-recovery:"+p.LocalTarget {
			return "", errors.New("SQL本地实例标记不符")
		}
	}
	meta, e := read("route_meta", "SELECT * FROM `"+p.RouteSchema+"`.route_meta WHERE id=1")
	if e != nil || len(meta) != 1 || string(meta[0].value("writable")) != "0" {
		return "", errors.New("全局SQL路由必须仍冻结")
	}
	reconcile, e := read("route_reconcile", "SELECT * FROM `"+p.RouteSchema+"`.route_reconcile WHERE id=1")
	if e != nil || len(reconcile) != 1 {
		return "", errors.New("路由对账水位缺失")
	}
	if rows, e := read("merged_into", "SELECT * FROM `"+p.RouteSchema+"`.zone_merged_into WHERE src_zone=?", p.Zone); e != nil || len(rows) != 0 {
		return "", errors.New("home zone已合走")
	}
	if rows, e := read("active_merge", "SELECT * FROM `"+p.RouteSchema+"`.zone_merge_fence WHERE zone_id=? AND run_id IS NOT NULL AND expires_at_ms>CAST(UNIX_TIMESTAMP(NOW(3))*1000 AS UNSIGNED)", p.Zone); e != nil || len(rows) != 0 {
		return "", errors.New("仍有SQL合服围栏")
	}
	routeByPlayer := map[uint64]releaseSQLRecord{}
	// 全zone真实路由成员与审定成员逐键等集；不允许仅恢复子集后解除整zone。
	roster, e := read("zone_members", "SELECT * FROM `"+p.RouteSchema+"`.player_route WHERE home_zone=? ORDER BY route_shard,player_id", p.Zone)
	if e != nil || len(roster) != len(p.Members) {
		return "", errors.New("当前zone完整成员数不符")
	}
	selected := map[uint64]MaintenanceReleaseMember{}
	for _, m := range p.Members {
		selected[m.PlayerID] = m
	}
	for _, row := range roster {
		id, e := positive(string(row.value("player_id")))
		m, ok := selected[id]
		storage, known := placement.StorageIDFromDBName(m.RowSchema)
		home, hasHome := row.cell("home_zone")
		place, hasPlace := row.cell("storage_id")
		version, hasVersion := row.cell("placement_version")
		frozen, hasFrozen := row.cell("frozen_run_id")
		if e != nil || !ok || !known || !hasHome || home.Null || !hasPlace || !hasVersion || !hasFrozen || place.Null != version.Null || string(home.Bytes) != strconv.FormatUint(uint64(p.Zone), 10) || !frozen.Null && len(frozen.Bytes) != 0 {
			return "", errors.New("当前路由身份/搬库状态/NULL配对不符")
		}
		if place.Null {
			if storage != p.Zone {
				return "", errors.New("无placement只允许本home库")
			}
		} else {
			actual, aerr := positive(string(place.Bytes))
			_, verr := positive(string(version.Bytes))
			if aerr != nil || verr != nil || actual != uint64(storage) {
				return "", errors.New("实际落点与成员SQL库不符")
			}
		}
		routeByPlayer[id] = row
		delete(selected, id)
	}
	if len(selected) != 0 {
		return "", errors.New("实际成员遗漏/重复")
	}
	promotions, e := read("maintenance_all", "SELECT * FROM `"+p.RouteSchema+"`.save_barrier_maintenance_receipt ORDER BY receipt_id")
	if e != nil {
		return "", e
	}
	matching := map[uint64]releaseSQLRecord{}
	for _, row := range promotions {
		src := &databasepb.SaveBarrierMaintenanceReceipt_PromotionPlan{}
		if proto.Unmarshal(row.value("plan_proto"), src) != nil {
			return "", errors.New("永久promotion原件损坏")
		}
		if src.MaintenanceId == p.MaintenanceID {
			if _, dup := matching[src.PlayerId]; dup {
				return "", errors.New("本维护重复成员promotion")
			}
			matching[src.PlayerId] = row
		}
	}
	for _, m := range p.Members {
		id := strconv.FormatUint(m.PlayerID, 10)
		for _, table := range []string{"player_database", "player_database_1"} {
			rows, e := read(table+"/"+id, "SELECT * FROM `"+m.RowSchema+"`."+table+" WHERE player_id=?", m.PlayerID)
			if e != nil || len(rows) != 1 {
				return "", errors.New("原双SQL不完整")
			}
			want := m.FirstBusinessSHA256
			if table == "player_database_1" {
				want = m.SecondBusinessSHA256
			}
			if string(rows[0].value("player_id")) != id || string(rows[0].value("fence_owner_epoch")) != strconv.FormatUint(m.Owner, 10) || string(rows[0].value("fence_save_seq")) != strconv.FormatUint(m.SaveSeq, 10) || releaseBusinessDigest(rows[0]) != want {
				return "", errors.New("SQL业务/NULL/未知blob/版本不等A原件")
			}
		}
		// 原完整zone中未做promotion的成员也不能留下开放P/旧admission；不以恢复子集冒全zone可解除。
		roleRows, e := read("role/"+id, "SELECT * FROM `"+p.RouteSchema+"`.account_role WHERE player_id=?", m.PlayerID)
		if e != nil || len(roleRows) != 1 || string(roleRows[0].value("trade_listing_id")) != "0" {
			return "", errors.New("全zone仍有未完成P")
		}
		admissionRows, e := read("admissions/"+id, "SELECT * FROM `"+p.RouteSchema+"`.account_role_admission WHERE player_id=? ORDER BY scene_instance_id", m.PlayerID)
		if e != nil {
			return "", e
		}
		for _, row := range admissionRows {
			if !bytes.Equal(row.value("highest_generation"), row.value("closed_through")) {
				return "", errors.New("全zone仍有开放admission")
			}
		}
		if len(m.PromotionProto) == 0 {
			if _, exists := matching[m.PlayerID]; exists {
				return "", errors.New("遗漏本维护永久promotion")
			}
			continue
		}
		src := &databasepb.SaveBarrierMaintenanceReceipt_PromotionPlan{}
		_ = proto.Unmarshal(m.PromotionProto, src)
		_, digest, _ := PromotionDigest(src)
		route := routeByPlayer[m.PlayerID]
		place, _ := route.cell("storage_id")
		if string(meta[0].value("generation")) != src.RouteGeneration || string(reconcile[0].value("watermark_ms")) != strconv.FormatUint(src.RouteWatermark, 10) || string(route.value("rev")) != strconv.FormatUint(src.RouteRevision, 10) {
			return "", errors.New("当前路由generation/watermark/rev偏离原promotion")
		}
		if place.Null {
			if src.StorageId != src.HomeZone || src.PlacementVersion != 0 {
				return "", errors.New("原promotion无落点语义不符")
			}
		} else if string(place.Bytes) != strconv.FormatUint(uint64(src.StorageId), 10) || string(route.value("placement_version")) != strconv.FormatUint(src.PlacementVersion, 10) {
			return "", errors.New("当前placement/version偏离原promotion")
		}
		if src.RedisAddr != r.Redis.Options().Addr || int(src.RedisDb) != r.Redis.Options().DB || src.MappingAddr != r.mapping().Options().Addr || int(src.MappingDb) != r.mapping().Options().DB {
			return "", errors.New("promotion不是当前原Redis目标")
		}

		rec, ok := matching[m.PlayerID]
		if !ok || !bytes.Equal(rec.value("receipt_id"), src.ReceiptId) || !bytes.Equal(rec.value("request_digest"), digest) || !bytes.Equal(rec.value("plan_proto"), m.PromotionProto) || string(rec.value("phase")) != strconv.Itoa(int(databasepb.SaveBarrierMaintenanceReceipt_BARRIER_CONFIRMED)) {
			return "", errors.New("永久promotion未确认或换原件")
		}
		delete(matching, m.PlayerID)
		recoveryRows, e := read("recoveries/"+id, "SELECT * FROM `"+p.RouteSchema+"`.account_role_admission_recovery WHERE maintenance_receipt_id=? ORDER BY request_id", src.ReceiptId)
		if e != nil || len(recoveryRows) != len(m.RecoveryProtos) {
			return "", errors.New("永久退休全集不符")
		}
		recoverySet := map[string]releaseSQLRecord{}
		for _, row := range recoveryRows {
			key := hex.EncodeToString(row.value("request_id"))
			if len(key) != 64 {
				return "", errors.New("SQL退休request_id损坏")
			}
			if _, duplicate := recoverySet[key]; duplicate {
				return "", errors.New("SQL退休request_id重复")
			}
			recoverySet[key] = row
		}
		for _, raw := range m.RecoveryProtos {
			expected := &databasepb.AccountRoleAdmissionRecovery{}
			if proto.Unmarshal(raw, expected) != nil || expected.PlayerId != m.PlayerID || !bytes.Equal(expected.MaintenanceReceiptId, src.ReceiptId) || !bytes.Equal(expected.MaintenanceRequestDigest, digest) {
				return "", errors.New("退休PB不属于原promotion")
			}
			key := hex.EncodeToString(expected.RequestId)
			row, found := recoverySet[key]
			if !found {
				return "", errors.New("退休原件重复/遗漏")
			}
			actual, err := releaseRecoveryProto(row)
			if err != nil || !proto.Equal(actual, expected) {
				return "", errors.New("永久退休原件已变")
			}
			delete(recoverySet, key)
		}
		if len(recoverySet) != 0 {
			return "", errors.New("仍有未消费的永久退休原件")
		}

		cancelRequest, e := readMaintenanceReleaseCancel(m.CancelAction)
		if e != nil || cancelRequest.PlayerId != m.PlayerID {
			return "", errors.New("缺正式原Cancel调用/实际回包")
		}
		for _, raw := range m.RecoveryProtos {
			recovery := &databasepb.AccountRoleAdmissionRecovery{}
			_ = proto.Unmarshal(raw, recovery)
			if recovery.ListingId != cancelRequest.ListingId || recovery.OwnerRevision != cancelRequest.ExpectedOwnerRevision || recovery.RequiredOwnerEpoch != m.Owner || recovery.RequiredSaveSeq != m.SaveSeq {
				return "", errors.New("退休原件与正式Cancel的listing/revision/原SQL围栏不符")
			}
		}
		rows := roleRows
		if string(rows[0].value("account")) != cancelRequest.SellerAccount || string(rows[0].value("owner_revision")) != strconv.FormatUint(cancelRequest.ExpectedOwnerRevision, 10) {
			return "", errors.New("正式Cancel后原归属不符")
		}
		rows, e = read("trade/"+id, "SELECT * FROM `"+p.RouteSchema+"`.account_role_trade WHERE listing_id=?", cancelRequest.ListingId)
		if e != nil || len(rows) != 1 || string(rows[0].value("player_id")) != id || string(rows[0].value("state")) != strconv.Itoa(int(dspb.AccountRoleTradeState_ACCOUNT_ROLE_TRADE_STATE_CANCELLED)) || string(rows[0].value("seller_account")) != cancelRequest.SellerAccount || string(rows[0].value("seller_owner_revision")) != strconv.FormatUint(cancelRequest.ExpectedOwnerRevision, 10) {
			return "", errors.New("当前SQL不是原listing的正式CANCELLED")
		}
		rows, e = read("holders/"+id, "SELECT * FROM `"+p.RouteSchema+"`.account_role_trade_holder WHERE listing_id=? ORDER BY scene_instance_id", cancelRequest.ListingId)
		if e != nil || len(rows) == 0 {
			return "", errors.New("原holder全集缺失")
		}
		for _, row := range rows {
			if string(row.value("drained")) != "1" {
				return "", errors.New("仍有未退休holder")
			}
		}

	}
	if len(matching) != 0 {
		return "", errors.New("本维护存在未列出的promotion成员")
	}
	if err = tx.Rollback(); err != nil {
		return "", err
	}
	raw, err := json.Marshal(evidence)
	if err != nil {
		return "", err
	}
	h := sha256.Sum256(raw)
	return hex.EncodeToString(h[:]), nil
}

type releaseSQLCell struct {
	Name  string `json:"name"`
	Null  bool   `json:"null"`
	Bytes []byte `json:"bytes"`
}
type releaseSQLRecord struct {
	Scope string           `json:"scope"`
	Cells []releaseSQLCell `json:"cells"`
}

func (row releaseSQLRecord) value(name string) []byte {
	for _, c := range row.Cells {
		if c.Name == name {
			return c.Bytes
		}
	}
	return nil
}
func (row releaseSQLRecord) cell(name string) (releaseSQLCell, bool) {
	for _, c := range row.Cells {
		if c.Name == name {
			return c, true
		}
	}
	return releaseSQLCell{}, false
}
func readMaintenanceReleaseRows(ctx context.Context, tx *sql.Tx, label, q string, args ...any) ([]releaseSQLRecord, error) {
	rows, err := tx.QueryContext(ctx, q, args...)
	if err != nil {
		return nil, err
	}
	defer rows.Close()
	columns, err := rows.Columns()
	if err != nil || len(columns) == 0 || len(columns) > 256 {
		return nil, errors.New("SQL列预算无效")
	}
	seen := map[string]bool{}
	for _, c := range columns {
		if seen[c] {
			return nil, errors.New("SQL重复列")
		}
		seen[c] = true
	}
	out := []releaseSQLRecord{}
	total := 0
	for rows.Next() {
		if len(out) >= 10000 {
			return nil, errors.New("SQL成员预算超限")
		}
		values := make([]sql.RawBytes, len(columns))
		dest := make([]any, len(columns))
		for i := range values {
			dest[i] = &values[i]
		}
		if err = rows.Scan(dest...); err != nil {
			return nil, err
		}
		r := releaseSQLRecord{Scope: label}
		for i, c := range columns {
			total += len(values[i])
			if total > 64<<20 {
				return nil, errors.New("SQL原件预算超限")
			}
			r.Cells = append(r.Cells, releaseSQLCell{Name: c, Null: values[i] == nil, Bytes: bytes.Clone(values[i])})
		}
		out = append(out, r)
	}
	return out, rows.Err()
}
func releaseMemberSchemas(members []MaintenanceReleaseMember) []string {
	seen := map[string]bool{}
	out := []string{}
	for _, m := range members {
		if !seen[m.RowSchema] {
			seen[m.RowSchema] = true
			out = append(out, m.RowSchema)
		}
	}
	sort.Strings(out)
	return out
}
func releaseBusinessDigest(row releaseSQLRecord) string {
	h := sha256.New()
	// 与本文件readRow的长度前缀/NULL规则完全一致，保物理列序；只排除两个fence列。
	for _, cell := range row.Cells {
		if cell.Name == "fence_owner_epoch" || cell.Name == "fence_save_seq" {
			continue
		}
		writePromotionPart(h, []byte(cell.Name))
		if cell.Null {
			h.Write([]byte{0})
		} else {
			h.Write([]byte{1})
			writePromotionPart(h, cell.Bytes)
		}
	}
	return hex.EncodeToString(h.Sum(nil))
}
func releaseRecoveryProto(row releaseSQLRecord) (*databasepb.AccountRoleAdmissionRecovery, error) {
	r := &databasepb.AccountRoleAdmissionRecovery{RequestId: bytes.Clone(row.value("request_id")), RequestDigest: bytes.Clone(row.value("request_digest")), RequestProto: bytes.Clone(row.value("request_proto")), MaintenanceReceiptId: bytes.Clone(row.value("maintenance_receipt_id")), MaintenanceRequestDigest: bytes.Clone(row.value("maintenance_request_digest")), SqlProofDigest: bytes.Clone(row.value("sql_proof_digest")), SceneInstanceId: string(row.value("scene_instance_id"))}
	for name, dest := range map[string]*uint64{"listing_id": &r.ListingId, "player_id": &r.PlayerId, "through_generation": &r.ThroughGeneration, "owner_revision": &r.OwnerRevision, "required_owner_epoch": &r.RequiredOwnerEpoch, "required_save_seq": &r.RequiredSaveSeq, "recovered_ms": &r.RecoveredMs} {
		n, e := positive(string(row.value(name)))
		if e != nil {
			return nil, e
		}
		*dest = n
	}
	request := &databasepb.AccountRoleAdmissionRecovery_Request{}
	// 与DS正式readAdmissionRecovery合同同义，校原件自身，而非只比较调用方提供的PB。
	if len(r.RequestId) != 32 || len(r.RequestProto) > 4096 || proto.Unmarshal(r.RequestProto, request) != nil || len(r.RequestDigest) != 32 || len(r.SqlProofDigest) != 32 ||
		!bytes.Equal(request.RequestId, r.RequestId) || request.GetSeal().GetListingId() != r.ListingId || request.GetSeal().GetPlayerId() != r.PlayerId || request.GetSeal().GetSceneInstanceId() != r.SceneInstanceId || request.GetSeal().GetThroughGeneration() != r.ThroughGeneration || request.GetSeal().GetOwnerRevision() != r.OwnerRevision ||
		!bytes.Equal(request.MaintenanceReceiptId, r.MaintenanceReceiptId) || !bytes.Equal(request.MaintenanceRequestDigest, r.MaintenanceRequestDigest) || !accountdirectory.ValidSaveFloor(r.RequiredOwnerEpoch, r.RequiredSaveSeq) || r.RequiredOwnerEpoch == 0 {
		return nil, errors.New("永久退休请求/SQL列/围栏或摘要长度不自洽")
	}
	h := sha256.New()
	writePromotionPart(h, []byte("account-role-admission-recovery-v1"))
	writePromotionPart(h, r.RequestProto)
	if !bytes.Equal(h.Sum(nil), r.RequestDigest) {
		return nil, errors.New("退休请求域摘要损坏")
	}
	return r, nil
}

// MaintenanceReleasePermit不暴露可写的授权布尔。唯一构造器读取审定source/实际PE/native原件，
// 并把短排他窗转成本进程单调截止；窗口不是SQL对外部进程的保证。
type MaintenanceReleasePermit struct {
	review       ReleaseFile
	input        ReleaseFile
	members      MaintenanceReleaseInput
	planSHA      string
	runID        string
	deadline     time.Time
	expiresMS    int64
	redisTargets []maintenanceReleaseReviewedRedisTarget
}
type MaintenanceReleaseInput struct {
	Format        string                     `json:"format"`
	Zone          uint32                     `json:"zone"`
	MaintenanceID string                     `json:"maintenance_id"`
	Members       []MaintenanceReleaseMember `json:"members"`
}
type maintenanceReleaseProducer struct {
	Role       string      `json:"role"`
	Schema     string      `json:"schema"`
	Kind       string      `json:"kind"`
	Source     ReleaseFile `json:"source"`
	Executable ReleaseFile `json:"executable"`
	BuildProof ReleaseFile `json:"build_proof"`
}
type maintenanceReleaseFact struct {
	Role          string        `json:"role"`
	Actual        ReleaseFile   `json:"actual"`
	NativeRecords []ReleaseFile `json:"native_records"`
}
type maintenanceReleaseReview struct {
	Format         string                                  `json:"schema"`
	RunID          string                                  `json:"run_id"`
	MembersInput   ReleaseFile                             `json:"members_input"`
	Producers      []maintenanceReleaseProducer            `json:"producers"`
	StableFacts    []maintenanceReleaseFact                `json:"stable_facts"`
	KafkaInventory ReleaseFile                             `json:"kafka_inventory"`
	RedisTargets   []maintenanceReleaseReviewedRedisTarget `json:"redis_targets"`
}
type maintenanceReleaseAttempt struct {
	Format     string                   `json:"schema"`
	Review     ReleaseFile              `json:"review"`
	RunID      string                   `json:"run_id"`
	PlanSHA256 string                   `json:"plan_sha256"`
	Window     ReleaseFile              `json:"window"`
	Facts      []maintenanceReleaseFact `json:"facts"`
}
type maintenanceReleaseWindow struct {
	Format     string      `json:"schema"`
	RunID      string      `json:"run_id"`
	PlanSHA256 string      `json:"plan_sha256"`
	Review     ReleaseFile `json:"review"`
	IssuedMS   int64       `json:"issued_ms"`
	ExpiresMS  int64       `json:"expires_ms"`
	// 原coordinator关闭发布入口/所有原entry退出的完整原件，而非writersStopped=true。
	NativeRecords []ReleaseFile `json:"native_records"`
}

func ReadMaintenanceReleaseMembers(pin ReleaseFile) (MaintenanceReleaseInput, error) {
	var result MaintenanceReleaseInput
	err := readReleaseJSON(pin, &result)
	if err != nil || result.Format != "maintenance-release-member-input-v1" || result.Zone == 0 || result.MaintenanceID == "" || len(result.Members) == 0 || len(result.Members) > 1000 {
		return result, errors.New("解除成员输入缺完整审定原件")
	}
	return result, nil
}

// ReadMaintenanceReleasePermit只读本地原件；调用它不代表任何SQL/Redis事实已采集。
// 未来actual缺失时明确拒绝。schema/source/PE/Build/native/输出逐层绑定，不取passed作事实。
func ReadMaintenanceReleasePermit(ctx context.Context, reviewPin, attemptPin ReleaseFile, expectedPlanSHA string) (*MaintenanceReleasePermit, error) {
	if err := ctx.Err(); err != nil {
		return nil, err
	}
	var review maintenanceReleaseReview
	var attempt maintenanceReleaseAttempt
	if readReleaseJSON(reviewPin, &review) != nil || readReleaseJSON(attemptPin, &attempt) != nil || review.Format != "maintenance-release-reviewed-producers-v1" || attempt.Format != "maintenance-release-fresh-attempt-v1" || review.RunID == "" || attempt.RunID != review.RunID || attempt.Review != reviewPin || attempt.PlanSHA256 != expectedPlanSHA {
		return nil, errors.New("审定release review/本次attempt/原计划不符")
	}
	input, err := ReadMaintenanceReleaseMembers(review.MembersInput)
	if err != nil {
		return nil, err
	}
	var window maintenanceReleaseWindow
	if readReleaseJSON(attempt.Window, &window) != nil || window.Format != "owned-recovery-explicit-release-window-v1" || window.RunID != review.RunID || window.Review != reviewPin || window.PlanSHA256 != expectedPlanSHA || window.ExpiresMS <= window.IssuedMS || window.ExpiresMS-window.IssuedMS > 60000 {
		return nil, errors.New("缺原计划的新短排他窗原件")
	}
	now := time.Now()
	if window.IssuedMS > now.UnixMilli() || window.ExpiresMS <= now.UnixMilli() {
		return nil, errors.New("排他维护窗尚未开始或已过期")
	}
	if len(window.NativeRecords) == 0 {
		return nil, errors.New("排他窗缺所有原entry退出原件，不采纳声明布尔")
	}
	for _, pin := range window.NativeRecords {
		if err := verifyMaintenanceReleaseStoppedRecord(pin); err != nil {
			return nil, err
		}
	}
	producers := map[string]maintenanceReleaseProducer{}
	for _, producer := range review.Producers {
		if producers[producer.Role].Role != "" || producer.Role == "" || producer.Schema == "" {
			return nil, errors.New("解除producer重复/缺contract")
		}
		if _, err = readReleaseBound(producer.Source, 16<<20); err != nil {
			return nil, err
		}
		if _, err = readReleaseBound(producer.Executable, 256<<20); err != nil {
			return nil, err
		}
		if producer.Kind == "go-specific" {
			if err = verifyMaintenanceReleaseBuild(producer); err != nil {
				return nil, err
			}
		} else if producer.Kind != "coordinator-kafka-readonly" {
			return nil, errors.New("未实现的解除事实producer类型，拒绝占位")
		}
		if producer.Role == "kafka-queues" {
			if producer.Kind != "coordinator-kafka-readonly" {
				return nil, errors.New("Kafka只能原CLI coordinator producer")
			}
		} else if producer.Kind != "go-specific" {
			return nil, errors.New("cutoff/Redis/Cancel必须实际独立Go PE")
		}
		producers[producer.Role] = producer
	}
	for _, fact := range review.StableFacts {
		if fact.Role == "redis-queues" || fact.Role == "kafka-queues" {
			return nil, errors.New("队列actual只能在本次fresh attempt，不准StableFacts复用")
		}
	}
	fresh := map[string]bool{}
	for _, fact := range attempt.Facts {
		if fact.Role != "redis-queues" && fact.Role != "kafka-queues" {
			return nil, errors.New("fresh事实仅两类队列观察")
		}
		if fresh[fact.Role] {
			return nil, errors.New("fresh队列事实重复")
		}
		fresh[fact.Role] = true
		if err = verifyMaintenanceReleaseFreshNative(fact, window); err != nil {
			return nil, err
		}
	}
	if !fresh["redis-queues"] || !fresh["kafka-queues"] {
		return nil, errors.New("每次attempt必须新的两类队列完整观察")
	}
	seen := map[string]bool{}
	for _, fact := range append(append([]maintenanceReleaseFact{}, review.StableFacts...), attempt.Facts...) {
		producer, ok := producers[fact.Role]
		if !ok || seen[fact.Role] {
			return nil, errors.New("解除事实遗漏/重复/未知producer")
		}
		seen[fact.Role] = true
		if err = verifyMaintenanceReleaseFact(producer, fact, review.RunID); err != nil {
			return nil, err
		}
		if fact.Role == "redis-queues" {
			if err = verifyMaintenanceReleaseRedisClosure(review, fact); err != nil {
				return nil, err
			}
		}
		if fact.Role == "kafka-queues" {
			if err = verifyMaintenanceReleaseKafkaClosure(review, fact); err != nil {
				return nil, err
			}
		}
	}
	for _, role := range []string{"issuer-cutoff", "redis-queues", "kafka-queues"} {
		if !seen[role] {
			return nil, errors.New("缺全部issuer/Redis/Kafka真实原件")
		}
	}
	if len(seen) != len(producers) {
		return nil, errors.New("审定producer全集与实际fact不等")
	}
	// 所有成员的Cancel action也须是本review实际caller的产物，不接受任意JSON自报。
	for _, member := range input.Members {
		if len(member.PromotionProto) > 0 {
			role := "trade-cancel/" + strconv.FormatUint(member.PlayerID, 10)
			if !seen[role] {
				return nil, errors.New("恢复成员缺本轮实际TradeClient Cancel producer")
			}
			found := false
			for _, fact := range review.StableFacts {
				if fact.Role == role && fact.Actual == member.CancelAction {
					found = true
				}
			}
			if !found {
				return nil, errors.New("成员Cancel不是审定原actual")
			}
		}
	}
	return &MaintenanceReleasePermit{review: reviewPin, input: review.MembersInput, members: input, planSHA: expectedPlanSHA, runID: review.RunID, deadline: now.Add(time.Duration(window.ExpiresMS-now.UnixMilli()) * time.Millisecond), expiresMS: window.ExpiresMS, redisTargets: review.RedisTargets}, nil
}
func (p *MaintenanceReleasePermit) check(plan MaintenanceReleasePlan, sha string) error {
	if p == nil || p.deadline.IsZero() || time.Until(p.deadline) <= 0 || p.planSHA != sha || p.review != plan.Review || p.members.Zone != plan.Zone || p.members.MaintenanceID != plan.MaintenanceID {
		return errors.New("解除短窗/原计划/审定维护已失效")
	}
	a, _ := json.Marshal(p.members.Members)
	b, _ := json.Marshal(plan.Members)
	if !bytes.Equal(a, b) {
		return errors.New("解除计划成员不是原审定完整输入")
	}
	if len(p.redisTargets) != len(plan.Targets) {
		return errors.New("计划物理target不是审定全集")
	}
	seen := map[string]bool{}
	for _, target := range plan.Targets {
		found := false
		for _, expected := range p.redisTargets {
			if expected.Addr == target.Addr && expected.DB == target.DB && expected.InstanceNonce == target.InstanceNonce {
				for _, zone := range expected.Zones {
					if zone == plan.Zone {
						found = true
					}
				}
			}
		}
		key := fmt.Sprintf("%s/%d", target.Addr, target.DB)
		if !found || seen[key] {
			return errors.New("计划addr/DB/nonce/zone偏离原审定")
		}
		seen[key] = true
	}
	return nil
}
func readReleaseBound(pin ReleaseFile, max int64) ([]byte, error) {
	if !filepath.IsAbs(pin.Path) || !releaseHex(pin.SHA256, 64) {
		return nil, errors.New("原件必须绝对路径和小写完整SHA")
	}
	clean := filepath.Clean(pin.Path)
	for path := clean; ; path = filepath.Dir(path) {
		st, err := os.Lstat(path)
		if err != nil || st.Mode()&os.ModeSymlink != 0 {
			return nil, errors.New("原件或祖先缺失/软链，拒绝越界替身")
		}
		if path == filepath.Dir(path) {
			break
		}
	}
	f, err := os.Open(clean)
	if err != nil {
		return nil, err
	}
	defer f.Close()
	st, err := f.Stat()
	if err != nil || !st.Mode().IsRegular() || st.Size() > max {
		return nil, errors.New("原件不是预算内普通文件")
	}
	raw, err := io.ReadAll(io.LimitReader(f, max+1))
	if err != nil || int64(len(raw)) > max {
		return nil, errors.New("原件读取超预算")
	}
	h := sha256.Sum256(raw)
	if hex.EncodeToString(h[:]) != pin.SHA256 {
		return nil, errors.New("原件SHA不符")
	}
	return raw, nil
}
func readReleaseJSON(pin ReleaseFile, out any) error {
	raw, err := readReleaseBound(pin, 64<<20)
	if err != nil {
		return err
	}
	d := json.NewDecoder(bytes.NewReader(raw))
	d.DisallowUnknownFields()
	if err = d.Decode(out); err != nil {
		return err
	}
	var extra any
	if d.Decode(&extra) != io.EOF {
		return errors.New("JSON尾部有第二正文")
	}
	return nil
}
func readReleaseObject(pin ReleaseFile) (map[string]json.RawMessage, error) {
	raw, err := readReleaseBound(pin, 64<<20)
	if err != nil {
		return nil, err
	}
	var result map[string]json.RawMessage
	if json.Unmarshal(raw, &result) != nil || result == nil {
		return nil, errors.New("实际原件不是JSON对象")
	}
	return result, nil
}
func releaseString(v map[string]json.RawMessage, key string) string {
	var out string
	_ = json.Unmarshal(v[key], &out)
	return out
}
func verifyMaintenanceReleaseStoppedRecord(pin ReleaseFile) error {
	v, err := readReleaseObject(pin)
	if err != nil {
		return err
	}
	if releaseString(v, "schema") != "owned-recovery-original-process-exit-v1" || releaseString(v, "creation_filetime") == "" || releaseString(v, "exit_filetime") == "" {
		return errors.New("窗口缺原句柄原birth/exit原件")
	}
	var pid uint32
	if json.Unmarshal(v["pid"], &pid) != nil || pid == 0 {
		return errors.New("退出原件pid不符")
	}
	var native map[string]json.RawMessage
	if json.Unmarshal(v["native_record"], &native) != nil {
		return errors.New("退出原件缺native正文")
	}
	birth, err := positive(releaseString(v, "creation_filetime"))
	if err != nil {
		return err
	}
	exit, err := positive(releaseString(v, "exit_filetime"))
	if err != nil || exit < birth || releaseString(native, "process_creation_filetime_utc") != releaseString(v, "creation_filetime") {
		return errors.New("原entry birth/exit与native不符")
	}
	return checkMaintenanceReleaseNative(native, releaseString(v, "image"), false)
}
func checkMaintenanceReleaseNative(v map[string]json.RawMessage, program string, requireZero bool) error {
	var started bool
	var exit *int
	var timeout bool
	_ = json.Unmarshal(v["native_started"], &started)
	_ = json.Unmarshal(v["native_exit"], &exit)
	_ = json.Unmarshal(v["timed_out"], &timeout)
	if !started || exit == nil || requireZero && *exit != 0 || timeout || releaseString(v, "process_creation_filetime_utc") == "" || !strings.EqualFold(filepath.Clean(releaseString(v, "program")), filepath.Clean(program)) {
		return errors.New("事实producer没有原program/birth/明确退出")
	}
	for _, key := range []string{"cleanup_errors", "output_errors", "evidence_errors"} {
		var rows []json.RawMessage
		if json.Unmarshal(v[key], &rows) != nil || len(rows) != 0 {
			return errors.New("事实producer清理/证据不干净")
		}
	}
	return nil
}
func verifyMaintenanceReleaseBuild(p maintenanceReleaseProducer) error {
	v, err := readReleaseObject(p.BuildProof)
	if err != nil {
		return err
	}
	if releaseString(v, "schema") != "current-go-module-v4-proof" || releaseString(v, "kind") != "RuntimeBuild" {
		return errors.New("缺具名当前独立Build")
	}
	var passed bool
	var audit []json.RawMessage
	var first any
	if json.Unmarshal(v["passed"], &passed) != nil || !passed || json.Unmarshal(v["audit_errors"], &audit) != nil || len(audit) != 0 || json.Unmarshal(v["first_error"], &first) != nil || first != nil {
		return errors.New("Build整体失败/审计漂移/首错不能用native0覆盖")
	}
	var manifest []ReleaseFile
	if json.Unmarshal(v["manifest_pins"], &manifest) != nil {
		return errors.New("Build缺原首终封存")
	}
	required := map[string]bool{"before-source-diff.json": false, "after-source-diff.json": false, "final-source-diff.json": false, "before-dependency-diff.json": false, "after-dependency-diff.json": false}
	for _, pin := range manifest {
		if _, ok := required[filepath.Base(pin.Path)]; ok {
			if required[filepath.Base(pin.Path)] {
				return errors.New("Build重复首终diff原件")
			}
			raw, e := readReleaseBound(pin, 1<<20)
			if e != nil {
				return e
			}
			var diff []json.RawMessage
			if json.Unmarshal(raw, &diff) != nil || len(diff) != 0 {
				return errors.New("Build source/deps首终diff不为空")
			}
			required[filepath.Base(pin.Path)] = true
		}
	}
	for _, found := range required {
		if !found {
			return errors.New("Build遗漏首终source/deps diff原件")
		}
	}

	var rows []map[string]json.RawMessage
	if json.Unmarshal(v["native_results"], &rows) != nil || len(rows) != 1 {
		return errors.New("独立Build原生命令数不符")
	}
	// Build本身的go.exe由其原plan工具pin守卫；这里绑定确切test/build输出及唯一本source。
	if err = checkMaintenanceReleaseNative(rows[0], releaseString(rows[0], "program"), true); err != nil {
		return err
	}
	var args []string
	_ = json.Unmarshal(rows[0]["arguments"], &args)
	if len(args) != 5 || args[0] != "build" || args[1] != "-p=1" || args[2] != "-o" || !strings.EqualFold(filepath.Clean(args[3]), filepath.Clean(p.Executable.Path)) || !strings.EqualFold(filepath.Clean(args[4]), filepath.Clean(p.Source.Path)) {
		return errors.New("Build不是原source的独立PE")
	}
	var artifact struct {
		File       ReleaseFile     `json:"file"`
		SourceMain json.RawMessage `json:"source_main_files"`
	}
	if json.Unmarshal(v["runtime_executable"], &artifact) != nil || !strings.EqualFold(artifact.File.SHA256, p.Executable.SHA256) || !strings.EqualFold(filepath.Clean(artifact.File.Path), filepath.Clean(p.Executable.Path)) {
		return errors.New("actual Build与原PE pin不符")
	}
	var single ReleaseFile
	var sources []ReleaseFile
	if len(artifact.SourceMain) > 0 && artifact.SourceMain[0] == '[' {
		if json.Unmarshal(artifact.SourceMain, &sources) != nil || len(sources) != 1 {
			return errors.New("Build source_main_files不是唯一来源")
		}
		single = sources[0]
	} else if json.Unmarshal(artifact.SourceMain, &single) != nil {
		return errors.New("Build缺source_main_files")
	}
	if !strings.EqualFold(single.SHA256, p.Source.SHA256) || !strings.EqualFold(filepath.Clean(single.Path), filepath.Clean(p.Source.Path)) {
		return errors.New("Build时source字节不是审定producer")
	}
	return nil
}
func verifyMaintenanceReleaseFact(p maintenanceReleaseProducer, f maintenanceReleaseFact, run string) error {
	if len(f.NativeRecords) == 0 {
		return errors.New("缺实际fact对应原native row")
	}
	for _, pin := range f.NativeRecords {
		v, err := readReleaseObject(pin)
		if err != nil {
			return err
		}
		if err = checkMaintenanceReleaseNative(v, p.Executable.Path, true); err != nil {
			return err
		}
		if p.Kind == "go-specific" {
			if err = verifyMaintenanceReleaseOutputArguments(p, f, v); err != nil {
				return err
			}
		}
	}
	v, err := readReleaseObject(f.Actual)
	if err != nil {
		return err
	}
	if releaseString(v, "schema") != p.Schema {
		return errors.New("fact contract不是审定source输出")
	}
	if value := releaseString(v, "run_id"); value != "" && value != run {
		return errors.New("fact来自其他run")
	}
	switch {
	case p.Role == "issuer-cutoff":
		if p.Schema != "owned-recovery-sm-client-cutoff-v1" {
			return errors.New("cutoff contract不符")
		}
		var actions []struct {
			ID    string `json:"redis_client_id"`
			Count int    `json:"kill_count"`
		}
		if json.Unmarshal(v["exact_id_actions"], &actions) != nil {
			return errors.New("缺精确client动作记录")
		}
		seen := map[string]bool{}
		for _, a := range actions {
			if _, err := positive(a.ID); err != nil || seen[a.ID] || a.Count < 0 || a.Count > 1 {
				return errors.New("cutoff IDs不规范")
			}
			seen[a.ID] = true
		}
		var stop ReleaseFile
		if json.Unmarshal(v["issuer_stops"], &stop) != nil {
			return errors.New("缺原SM/proxy停止原件")
		}
		if _, err = readReleaseObject(stop); err != nil {
			return err
		}
	case p.Role == "redis-queues":
		if p.Schema != "owned-recovery-redis-complete-queue-drain-v1" {
			return errors.New("Redis队列contract不符")
		}
		var targets []struct {
			PendingBuckets int      `json:"pending_buckets"`
			Blockers       []string `json:"blockers"`
			Observations   []struct {
				Kind  string `json:"kind"`
				Count int64  `json:"count"`
			} `json:"observations"`
		}
		if json.Unmarshal(v["targets"], &targets) != nil || len(targets) == 0 {
			return errors.New("缺Redis实际全key观察")
		}
		for _, t := range targets {
			if t.PendingBuckets < 64 || len(t.Blockers) != 0 {
				return errors.New("64桶遗漏/Redis仍有阻断")
			}
			for _, o := range t.Observations {
				if o.Count < 0 || o.Kind != "barrier" && o.Kind != "instances" && o.Kind != "modes" && o.Count != 0 {
					return errors.New("Redis队列仍有未处理成员")
				}
			}
		}
	case p.Role == "kafka-queues":
		if p.Schema != "owned-recovery-kafka-all-reviewed-generations-drain-proof-v1" || p.Kind != "coordinator-kafka-readonly" {
			return errors.New("Kafka producer不符")
		}
		var rows []struct {
			Group     string `json:"group"`
			Topic     string `json:"topic"`
			Partition int    `json:"partition"`
			Earliest  int64  `json:"earliest"`
			Committed int64  `json:"committed"`
			End       int64  `json:"end"`
			Lag       int64  `json:"lag"`
		}
		if json.Unmarshal(v["partition_rows"], &rows) != nil || len(rows) == 0 {
			return errors.New("缺Kafka逐世代/订阅实际offset")
		}
		seen := map[string]bool{}
		for _, row := range rows {
			key := fmt.Sprintf("%s/%s/%d", row.Group, row.Topic, row.Partition)
			if row.Group == "" || row.Topic == "" || row.Partition < 0 || seen[key] || row.Earliest < 0 || row.Committed < row.Earliest || row.Committed != row.End || row.Lag != 0 {
				return errors.New("Kafka未提交/积压/分区重复")
			}
			seen[key] = true
		}
		var inventory ReleaseFile
		if json.Unmarshal(v["inventory"], &inventory) != nil {
			return errors.New("Kafka缺审定历史全集")
		}
		if _, err = readReleaseObject(inventory); err != nil {
			return err
		}
	case strings.HasPrefix(p.Role, "trade-cancel/"):
		_, err = readMaintenanceReleaseCancel(f.Actual)
		return err
	default:
		return errors.New("未知解除事实角色")
	}
	return nil
}

func verifyMaintenanceReleaseOutputArguments(p maintenanceReleaseProducer, f maintenanceReleaseFact, native map[string]json.RawMessage) error {
	var args []string
	if json.Unmarshal(native["arguments"], &args) != nil || len(args)%2 != 0 {
		return errors.New("事实producer参数不是固定键值对")
	}
	flags := map[string]string{}
	for i := 0; i < len(args); i += 2 {
		if flags[args[i]] != "" {
			return errors.New("事实producer重复参数")
		}
		flags[args[i]] = args[i+1]
	}
	wantMode := "queues"
	if p.Role == "issuer-cutoff" {
		wantMode = "cutoff"
	}
	if strings.HasPrefix(p.Role, "trade-cancel/") {
		wantMode = "cancel"
		if flags["-mode"] != wantMode || !strings.EqualFold(filepath.Clean(filepath.Join(flags["-out-dir"], "cancel-action.json")), filepath.Clean(f.Actual.Path)) {
			return errors.New("Cancel实际argv没产此原action")
		}
		return nil
	}
	if flags["-mode"] != wantMode || !strings.EqualFold(filepath.Clean(flags["-out"]), filepath.Clean(f.Actual.Path)) {
		return errors.New("actual原件不是该native参数的固定输出")
	}
	return nil
}

// 严格读取Root具名r6 producer的真实action、Begin、reply和post-state；不采信其passed布尔。
// 请求Auth不落盘；回复协议本来没有签名，绝不捏造reply-auth字段。
func readMaintenanceReleaseCancel(pin ReleaseFile) (*dspb.CancelAccountRoleTradeRequest, error) {
	v, err := readReleaseObject(pin)
	if err != nil {
		return nil, err
	}
	if releaseString(v, "schema") != "owned-old-scene-formal-trade-action-v1" || releaseString(v, "action") != "cancel" {
		return nil, errors.New("不是正式原Cancel action")
	}
	var requestRaw []byte
	if json.Unmarshal(v["unsigned_request_proto"], &requestRaw) != nil {
		return nil, errors.New("缺原unsigned请求")
	}
	sum := sha256.Sum256(requestRaw)
	if hex.EncodeToString(sum[:]) != releaseString(v, "unsigned_request_sha256") {
		return nil, errors.New("原Cancel请求摘要不符")
	}
	in := &dspb.CancelAccountRoleTradeRequest{}
	if proto.Unmarshal(requestRaw, in) != nil || in.Auth != nil || len(in.ProtoReflect().GetUnknown()) != 0 || in.PlayerId == 0 || in.ListingId == 0 || in.ExpectedOwnerRevision == 0 || !accountdirectory.ValidAccount(in.SellerAccount) {
		return nil, errors.New("原Cancel请求不是原tuple/有Auth泄漏")
	}
	var originalPin, replyPin, statePin, observationPin ReleaseFile
	for key, out := range map[string]*ReleaseFile{"original_begin_receipt": &originalPin, "reply": &replyPin, "post_state": &statePin, "original_observation": &observationPin} {
		if json.Unmarshal(v[key], out) != nil {
			return nil, errors.New("Cancel缺完整下游pins")
		}
	}
	begin := &dspb.AccountRoleTradeReceipt{}
	reply := &dspb.AccountRoleTradeReceipt{}
	for _, item := range []struct {
		pin ReleaseFile
		msg proto.Message
	}{{originalPin, begin}, {replyPin, reply}} {
		raw, e := readReleaseBound(item.pin, 1<<20)
		if e != nil || (protojson.UnmarshalOptions{DiscardUnknown: false}).Unmarshal(raw, item.msg) != nil {
			return nil, errors.New("原正式Begin/Cancel回包损坏")
		}
	}
	if begin.PlayerId != in.PlayerId || begin.ListingId != in.ListingId || begin.SellerAccount != in.SellerAccount || begin.SellerOwnerRevision != in.ExpectedOwnerRevision || reply.PlayerId != begin.PlayerId || reply.ListingId != begin.ListingId || reply.SellerAccount != begin.SellerAccount || reply.SellerOwnerRevision != begin.SellerOwnerRevision || reply.State != dspb.AccountRoleTradeState_ACCOUNT_ROLE_TRADE_STATE_CANCELLED || len(begin.Holders) == 0 || len(reply.Holders) != len(begin.Holders) {
		return nil, errors.New("正式Cancel换主体/原holder全集或非终态")
	}
	holders := map[string]*base.RoleTradeSeal{}
	for _, h := range begin.Holders {
		if h == nil || h.Seal == nil || holders[h.Seal.SceneInstanceId] != nil {
			return nil, errors.New("Begin holder损坏/重复")
		}
		holders[h.Seal.SceneInstanceId] = h.Seal
	}
	for _, h := range reply.Holders {
		if h == nil || h.Seal == nil || !h.Drained || !proto.Equal(holders[h.Seal.SceneInstanceId], h.Seal) {
			return nil, errors.New("Cancel未排空原holder/更换上界")
		}
		delete(holders, h.Seal.SceneInstanceId)
	}
	if len(holders) != 0 {
		return nil, errors.New("Cancel遗漏原holder")
	}
	// producer已对原全列业务/归属做真实只读对照；本core随后再从当前SQL独立复核终态/全列。
	if _, err = readReleaseObject(statePin); err != nil {
		return nil, err
	}
	if _, err = readReleaseObject(observationPin); err != nil {
		return nil, err
	}
	return in, nil
}

type maintenanceReleaseReviewedRedisTarget struct {
	Role          string   `json:"role"`
	Addr          string   `json:"addr"`
	DB            int      `json:"db"`
	InstanceNonce string   `json:"instance_nonce"`
	Zones         []uint32 `json:"zones"`
}
type maintenanceReleaseKafkaInventory struct {
	Schema string `json:"schema"`
	Topics []struct {
		Name       string `json:"name"`
		Partitions int    `json:"partitions"`
	} `json:"topics"`
	Subscriptions []struct {
		Group string `json:"group"`
		Topic string `json:"topic"`
	} `json:"subscriptions"`
	Internal []string `json:"internal_topics"`
}

func releaseInventory(pin ReleaseFile) (maintenanceReleaseKafkaInventory, error) {
	var inv maintenanceReleaseKafkaInventory
	raw, err := readReleaseBound(pin, 1<<20)
	if err != nil || json.Unmarshal(raw, &inv) != nil || inv.Schema != "owned-recovery-kafka-reviewed-inventory-v1" || len(inv.Topics) == 0 || len(inv.Topics) > 128 || len(inv.Subscriptions) == 0 {
		return inv, errors.New("缺审定Kafka真实历史全集")
	}
	return inv, nil
}
func verifyMaintenanceReleaseFreshNative(f maintenanceReleaseFact, w maintenanceReleaseWindow) error {
	for _, pin := range f.NativeRecords {
		v, err := readReleaseObject(pin)
		if err != nil {
			return err
		}
		birth, err := positive(releaseString(v, "process_creation_filetime_utc"))
		if err != nil || birth < 116444736000000000 {
			return errors.New("fresh观察原native birth不符")
		}
		birthMS := int64((birth - 116444736000000000) / 10000)
		end, err := time.Parse(time.RFC3339Nano, releaseString(v, "ended_utc"))
		if err != nil || birthMS < w.IssuedMS || end.UnixMilli() < birthMS || end.UnixMilli() > w.ExpiresMS {
			return errors.New("队列观察不是本次短窗新执行，拒绝复用历史passed")
		}
	}
	return nil
}
func verifyMaintenanceReleaseKafkaClosure(review maintenanceReleaseReview, f maintenanceReleaseFact) error {
	inv, err := releaseInventory(review.KafkaInventory)
	if err != nil {
		return err
	}
	v, err := readReleaseObject(f.Actual)
	if err != nil {
		return err
	}
	var pin ReleaseFile
	if json.Unmarshal(v["inventory"], &pin) != nil || pin != review.KafkaInventory {
		return errors.New("Kafka actual未用原审定inventory")
	}
	var names, groups []string
	if json.Unmarshal(v["topic_names"], &names) != nil || json.Unmarshal(v["group_names"], &groups) != nil {
		return errors.New("Kafka实际catalogue缺失")
	}
	topics := map[string]int{}
	expectedTopics := []string{}
	partitions := 0
	for _, t := range inv.Topics {
		if t.Name == "" || t.Partitions < 1 || t.Partitions > 4096 || topics[t.Name] != 0 {
			return errors.New("审定Kafka topic/partition重复或非法")
		}
		topics[t.Name] = t.Partitions
		expectedTopics = append(expectedTopics, t.Name)
		partitions += t.Partitions
	}
	if partitions > 8192 {
		return errors.New("Kafka分区预算超限")
	}
	for _, name := range inv.Internal {
		if name != "__consumer_offsets" {
			return errors.New("不能将未知业务topic伪装internal")
		}
		expectedTopics = append(expectedTopics, name)
	}
	if !releaseExactStrings(names, expectedTopics) {
		return errors.New("Kafka实际topic不是原审定全历史全集")
	}
	expectedRows := map[string]bool{}
	groupSet := map[string]bool{}
	for _, s := range inv.Subscriptions {
		n := topics[s.Topic]
		if s.Group == "" || n == 0 {
			return errors.New("审定订阅非法")
		}
		groupSet[s.Group] = true
		for p := 0; p < n; p++ {
			key := fmt.Sprintf("%s/%s/%d", s.Group, s.Topic, p)
			if expectedRows[key] {
				return errors.New("审定订阅重复")
			}
			expectedRows[key] = true
		}
	}
	if len(expectedRows) > 16384 {
		return errors.New("订阅分区预算超限")
	}
	expectedGroups := []string{}
	for g := range groupSet {
		expectedGroups = append(expectedGroups, g)
	}
	if !releaseExactStrings(groups, expectedGroups) {
		return errors.New("Kafka实际group全集不等")
	}
	var rows []struct {
		Group     string `json:"group"`
		Topic     string `json:"topic"`
		Partition int    `json:"partition"`
		Earliest  int64  `json:"earliest"`
		Committed int64  `json:"committed"`
		End       int64  `json:"end"`
		Lag       int64  `json:"lag"`
	}
	if json.Unmarshal(v["partition_rows"], &rows) != nil || len(rows) != len(expectedRows) {
		return errors.New("Kafka实际分区全集缺失")
	}
	for _, r := range rows {
		key := fmt.Sprintf("%s/%s/%d", r.Group, r.Topic, r.Partition)
		if !expectedRows[key] || r.Earliest < 0 || r.Committed < r.Earliest || r.Committed != r.End || r.Lag != 0 {
			return errors.New("Kafka分区重复/漏项/积压")
		}
		delete(expectedRows, key)
	}
	if len(expectedRows) != 0 {
		return errors.New("Kafka分区全集未消费")
	}
	return nil
}
func releaseExactStrings(a, b []string) bool {
	if len(a) != len(b) {
		return false
	}
	seen := map[string]bool{}
	for _, v := range b {
		if seen[v] {
			return false
		}
		seen[v] = true
	}
	for _, v := range a {
		if !seen[v] {
			return false
		}
		delete(seen, v)
	}
	return len(seen) == 0
}

type maintenanceReleaseRedisKeyObservation struct {
	KeySHA      string `json:"key_sha256"`
	Kind        string `json:"kind"`
	Topic       string `json:"topic,omitempty"`
	Type        string `json:"type"`
	DumpSHA     string `json:"dump_sha256"`
	PTTL        int64  `json:"pttl_ms"`
	Count       int64  `json:"count"`
	OutboxTopic string `json:"outbox_topic,omitempty"`
}

func verifyMaintenanceReleaseRedisClosure(review maintenanceReleaseReview, f maintenanceReleaseFact) error {
	inv, err := releaseInventory(review.KafkaInventory)
	if err != nil {
		return err
	}
	v, err := readReleaseObject(f.Actual)
	if err != nil {
		return err
	}
	var defPin ReleaseFile
	if json.Unmarshal(v["definition"], &defPin) != nil {
		return errors.New("Redis实际观察缺原definition")
	}
	var def struct {
		Schema  string `json:"schema"`
		Targets []struct {
			Role  string   `json:"role"`
			Addr  string   `json:"addr"`
			DB    int      `json:"db"`
			RunID string   `json:"expected_run_id"`
			Zones []uint32 `json:"zones"`
		} `json:"targets"`
		Topics []struct {
			Name       string `json:"name"`
			Partitions int    `json:"partitions"`
		} `json:"topics"`
	}
	raw, err := readReleaseBound(defPin, 1<<20)
	if err != nil || json.Unmarshal(raw, &def) != nil || def.Schema != "owned-recovery-redis-reviewed-inventory-v1" {
		return errors.New("Redis实际definition不是已知producer contract")
	}
	// definition的非策略字段由原producer强验；这里另核审定target/zone/topic全集，不能只看有若干观察。
	approved := map[string]maintenanceReleaseReviewedRedisTarget{}
	for _, target := range review.RedisTargets {
		key := fmt.Sprintf("%s/%d", target.Addr, target.DB)
		if target.Role == "" || approved[key].Role != "" || !promotionTargetID(target.InstanceNonce) || len(target.Zones) == 0 {
			return errors.New("审定Redis全集重复/非法")
		}
		approved[key] = target
	}
	if len(approved) == 0 || len(def.Targets) != len(approved) {
		return errors.New("Redis实际target全集数不等")
	}
	expectedTopics := map[string]int{}
	for _, t := range inv.Topics {
		expectedTopics[t.Name] = t.Partitions
	}
	if len(def.Topics) != len(expectedTopics) {
		return errors.New("Redis topic全集与Kafka历史世代不等")
	}
	for _, t := range def.Topics {
		if expectedTopics[t.Name] != t.Partitions {
			return errors.New("Redis topic分区合同不符")
		}
		delete(expectedTopics, t.Name)
	}
	if len(expectedTopics) != 0 {
		return errors.New("Redis漏历史topic")
	}
	var actual []struct {
		Role           string                                  `json:"role"`
		Addr           string                                  `json:"addr"`
		DB             int                                     `json:"db"`
		RunID          string                                  `json:"run_id"`
		KeyCount       int                                     `json:"key_count"`
		PendingBuckets int                                     `json:"pending_buckets"`
		Observations   []maintenanceReleaseRedisKeyObservation `json:"observations"`
		Digest         string                                  `json:"observations_sha256"`
		Blockers       []string                                `json:"blockers"`
	}
	if json.Unmarshal(v["targets"], &actual) != nil || len(actual) != len(approved) {
		return errors.New("Redis实际targets遗漏")
	}
	defs := map[string]int{}
	for i, t := range def.Targets {
		key := fmt.Sprintf("%s/%d", t.Addr, t.DB)
		a, ok := approved[key]
		if !ok || a.Role != t.Role || defs[key] != 0 || !releaseEqualZones(a.Zones, t.Zones) || len(t.RunID) != 40 || strings.Trim(t.RunID, "0123456789abcdef") != "" {
			return errors.New("Redis definition偏离原addr/DB/zone全集或epoch")
		}
		defs[key] = i + 1
	}
	for _, t := range actual {
		key := fmt.Sprintf("%s/%d", t.Addr, t.DB)
		index, ok := defs[key]
		if !ok {
			return errors.New("Redis实际target重复/未知")
		}
		definition := def.Targets[index-1]
		if t.Role != definition.Role || t.RunID != definition.RunID || t.PendingBuckets != 64*len(definition.Zones) || t.KeyCount != len(t.Observations) || len(t.Blockers) != 0 || len(t.Observations) > 16384 {
			return errors.New("Redis实际epoch/64桶/数量或阻断不符")
		}
		delete(defs, key)
		required := map[string]string{}
		add := func(key, kind string) { h := sha256.Sum256([]byte(key)); required[hex.EncodeToString(h[:])] = kind }
		for _, z := range definition.Zones {
			for b := 0; b < 64; b++ {
				add(fmt.Sprintf("player:save_pending:%d:%d", z, b), "pending")
			}
		}
		for _, topic := range inv.Topics {
			for _, pair := range []struct{ prefix, kind string }{{"kafka:retry:queue:", "retry"}, {"kafka:dead:queue:", "dead"}, {"kafka:retry:processing:", "processing"}, {"kafka:retry:instances:", "instances"}, {"kafka:retry:instance_mode:", "modes"}} {
				add(pair.prefix+topic.Name, pair.kind)
			}
			for p := 0; p < topic.Partitions; p++ {
				add(fmt.Sprintf("kafka:retry:queue:%s:p%d", topic.Name, p), "retry")
			}
		}
		seen := map[string]bool{}
		stable := append([]maintenanceReleaseRedisKeyObservation{}, t.Observations...)
		for i, o := range t.Observations {
			if !releaseHex(o.KeySHA, 64) || seen[o.KeySHA] || !releaseHex(o.DumpSHA, 64) || o.Count < 0 || o.OutboxTopic != "" {
				return errors.New("Redis原key摘要重复/未知待投")
			}
			seen[o.KeySHA] = true
			if kind, known := required[o.KeySHA]; known {
				if kind != o.Kind {
					return errors.New("必读Redis key类型角色不符")
				}
				delete(required, o.KeySHA)
			}
			switch o.Kind {
			case "retry", "dead", "processing", "pending":
				if o.Count != 0 {
					return errors.New("Redis仍有待办")
				}
			case "barrier", "instances", "modes":
			default:
				return errors.New("Redis未知队列分类")
			}
			stable[i].PTTL = 0
		}
		if len(required) != 0 {
			return errors.New("Redis缺固定64桶/topic/processing必读全集")
		}
		encoded, _ := json.Marshal(stable)
		sum := sha256.Sum256(encoded)
		if hex.EncodeToString(sum[:]) != t.Digest {
			return errors.New("Redis逐key观察摘要不符")
		}
	}
	if len(defs) != 0 {
		return errors.New("Redis实际target未闭合")
	}
	return nil
}
func releaseEqualZones(a, b []uint32) bool {
	aa := []string{}
	bb := []string{}
	for _, z := range a {
		aa = append(aa, strconv.FormatUint(uint64(z), 10))
	}
	for _, z := range b {
		bb = append(bb, strconv.FormatUint(uint64(z), 10))
	}
	return releaseExactStrings(aa, bb)
}
