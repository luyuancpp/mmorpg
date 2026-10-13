package placement

// 本文件是「映射持久化(D1)」在 Redis 一侧的契约:新增的两把键、合服围栏分片、rev 的格式与比较、
// 发布脚本(set-if-newer)全文及其回复分类(docs/design/player-route-mapping-durability-proposal.md §3.3、§4.2,
// 第 1a 批定稿)。
//
// 真源在全局库 player_route(data_service 的 store 包);mapping Redis 里的 player:zone / player:placement
// 只是它的副本,player:maprev 记录副本已经到了库里哪一版(rev),发布脚本按它做 set-if-newer。
//
// 与 placement.go 同一约定:只依赖标准库。Redis 的读写代码不放这里 —— data_service 与 tools/merge_zone
// 用的 Redis 客户端不同,各自用自己的客户端执行 PublishRouteScript,这里只给脚本、键和回复的判定口径。

import (
	"errors"
	"fmt"
	"strconv"
)

// MapRevKeyPrefix 是 player:maprev:{id} 的前缀:值是十进制 rev(无前导零),无 TTL。
// 只用于 set-if-newer 排序和审计;旧读方不认识它,也不受它影响。
const MapRevKeyPrefix = "player:maprev:"

// MapRevKey 返回 player:maprev:{id}。
func MapRevKey(playerID uint64) string {
	return MapRevKeyPrefix + strconv.FormatUint(playerID, 10)
}

// RouteReadyKey 是「副本完整可信」标记(提案 §3.3)。第 1a 批只定义,不写也不读:
// 写它的是第 1b 批的对账 leader,提前写会让读方误以为缓存缺键就是权威缺席。
const RouteReadyKey = "mapping:route:ready"

// 合服围栏在库里按 (zone_id, shard) 存 N 份相同内容:建角只锁 player_id % N 那一个分片,
// 获取围栏的一方锁全部分片(提案 §3.2「围栏为什么分片」)。
const (
	// DefaultRouteFenceShards 是 RouteStore.FenceShards 的缺省值。
	DefaultRouteFenceShards uint32 = 8
	// MaxRouteFenceShards 是分片数上限,同时是**围栏持有者的契约**:持有者(第 2 批的 merge_zone)
	// 必须锁住并写满 0..MaxRouteFenceShards-1 全部分片行,而不是只写自己配置的 N 行 ——
	// 各进程的 N 可能不同,N 大的写者会落到持有者没锁的分片上,绕过围栏。
	MaxRouteFenceShards uint32 = 64
)

// RouteFenceShard 返回 playerID 在 shards 个分片里落在哪一片。shards 不在 [1, MaxRouteFenceShards] 时报错:
// 0 会除零,超过上限则会落到围栏持有者不保证写过的分片上(见 MaxRouteFenceShards)。
func RouteFenceShard(playerID uint64, shards uint32) (uint32, error) {
	if shards < 1 || shards > MaxRouteFenceShards {
		return 0, fmt.Errorf("placement: route fence shards %d out of [1, %d]", shards, MaxRouteFenceShards)
	}
	return uint32(playerID % uint64(shards)), nil
}

// RouteRowShards 是 player_route 的固定分片数:route_shard = player_id % RouteRowShards,
// 是聚簇主键 (route_shard, player_id) 的首列。目的是把单调递增的 player_id 在 TiDB 上拆成 16 条写入尾部
// (docs/design/player-route-mapping-durability-proposal.md §11.2「第 1a 批热点修复」):号段发号在每个 login 段内
// 严格递增,只按 player_id 排序的主键 / 索引在开服首日始终只写同一个尾部 Region。
//
// **一旦任何环境建过 player_route 就不可改**:改模数 = 每一行换主键 = 整表重写;
// data_service store 包 schema.go 的 chk_route_shard 里写死的 16 与这里必须一致(route_store_test.go 钉住)。
// 与 RouteFenceShard 无关:那是可配置的围栏分片数,只决定 W1 锁哪一个围栏行;两者同时出现的函数里不得同名。
const RouteRowShards uint32 = 16

// RouteRowShard 返回 player_route.route_shard。写者与读者都只能经它计算(tools/merge_zone 已经 import
// shared/placement,第 2 批写库时直接调它,不得自己再写一份 % 16 —— 算错片的写者在 CHECK 不生效的库上
// 能给同一个 player_id 写出两行,而按正确片点查的读者看不到另一行)。
func RouteRowShard(playerID uint64) uint32 {
	return uint32(playerID % uint64(RouteRowShards))
}

// maxRevDigits 是 uint64 十进制的最大位数(18446744073709551615)。
const maxRevDigits = 20

// FormatRev 把库里的 rev 格式化成发布脚本要的十进制串。rev == 0 不是合法版本(chk_route_rev 要求 >= 1),
// 返回空串;调用方必须先拒绝 0,空串交给脚本会被当成坏参数拒掉,不会写进缓存。
func FormatRev(rev uint64) string {
	if rev == 0 {
		return ""
	}
	return strconv.FormatUint(rev, 10)
}

// ValidRev 判断 s 是否是规范的 rev:^[1-9][0-9]{0,19}$ 且不超过 MaxUint64。
//
// 规范形式(无前导零、无符号、无空白)是「先比长度再比字典序」成立的前提:允许 "010" 的话,
// "010" 与 "9" 的比较结果就错了。与 Lua 里的 valid_rev 同口径,只多一条 MaxUint64 上限
// (Lua 那边只靠写者纪律,脚本写进缓存的 rev 只可能来自本函数校验过的参数)。
func ValidRev(s string) bool {
	if len(s) < 1 || len(s) > maxRevDigits || s[0] < '1' || s[0] > '9' {
		return false
	}
	for i := 1; i < len(s); i++ {
		if s[i] < '0' || s[i] > '9' {
			return false
		}
	}
	// 字符集与长度都对时,ParseUint 只剩「超过 MaxUint64」一种失败。
	_, err := strconv.ParseUint(s, 10, 64)
	return err == nil
}

// CompareRev 比较两个 rev:a<b 回 -1,相等回 0,a>b 回 1。先比长度、再逐字节比,
// 与发布脚本的 rev_ge 同一判据(不经 double,也不依赖 locale)。任一方不规范时回 ErrMalformed。
func CompareRev(a, b string) (int, error) {
	if !ValidRev(a) || !ValidRev(b) {
		return 0, fmt.Errorf("%w: rev %q vs %q", ErrMalformed, a, b)
	}
	switch {
	case len(a) < len(b):
		return -1, nil
	case len(a) > len(b):
		return 1, nil
	case a < b:
		return -1, nil
	case a > b:
		return 1, nil
	default:
		return 0, nil
	}
}

// PublishRouteKeys 返回发布脚本的 KEYS:player:zone、player:placement、player:maprev,
// fenceZone != 0 时追加第 4 把 merge:in_progress:{fenceZone}(W1 用:提交点复核合服围栏);
// fenceZone == 0 只给三把(第 2 批合服批量发布用:那时围栏由持有者自己在库里校验)。
//
// 多键脚本成立的前提是 mapping Redis 为单实例(非 Cluster),所以键里不带 hash tag。
func PublishRouteKeys(playerID uint64, fenceZone uint32) []string {
	keys := []string{HomeZoneKey(playerID), Key(playerID), MapRevKey(playerID)}
	if fenceZone != 0 {
		keys = append(keys, MergeFenceKey(fenceZone))
	}
	return keys
}

// PublishRouteScript 是把库里一行发布到 mapping Redis 的脚本(set-if-newer)。
//
// 在提案 §4.2 的基础上加了五处(规格 b.1):rev 参数校验、可选的围栏键(KEYS[4],提交点复核合服围栏)、
// 无 maprev 时的遗留值守卫(不静默覆盖 P1 导入之前或绕过本脚本的写者留下的值)、返回 0 时把 maprev
// 原值写回一次(touch,让同一连接随后的 WAITAOF 覆盖「此刻已在缓存里的状态」)、逐字节比较。
//
// 用 EVAL 而不是 EVALSHA:pipeline 里遇到 NOSCRIPT 无法透明回退,且与 data_service 现有脚本
// (router.go 全部 EvalCtx)一致。不加 shebang:miniredis 是否兼容未核实。
//
// 回复形状与含义见 ClassifyPublishReply;改脚本必须同步改它和 route_test.go 的向量。
const PublishRouteScript = `-- route publish (set-if-newer),docs/design/player-route-mapping-durability-proposal.md §4.2(第 1a 批定稿)
-- KEYS: 1=player:zone:{id} 2=player:placement:{id} 3=player:maprev:{id} [4=merge:in_progress:{home},可选]
-- ARGV: 1=rev(十进制,无前导零,1..20 位) 2=home(''=DEL) 3=placement(''=DEL)
-- 返回: {1, rev}                          已写入三键
--       {0, cur, curHome, curPlacement}   缓存 rev >= 本次:映射未改(maprev 原值写回一次,见下)
--       {2, '', curHome, curPlacement}    缓存无 maprev 且遗留值与本次不同:零写入
--       {3}                               KEYS[4] 存在(合服围栏):零写入
local function valid_rev(s)
  return type(s) == 'string' and #s >= 1 and #s <= 20 and string.match(s, '^[1-9][0-9]*$') ~= nil
end
-- 两边都已过 valid_rev:先比长度再逐字节比。不经 Lua number(double),也不依赖 strcoll / locale。
local function rev_ge(a, b)
  if #a ~= #b then return #a > #b end
  for i = 1, #a do
    local x, y = string.byte(a, i), string.byte(b, i)
    if x ~= y then return x > y end
  end
  return true
end
local rev = ARGV[1]
if not valid_rev(rev) then
  return redis.error_reply('ERR route publish: bad rev argument')
end
-- 提交点复核合服围栏(与 router.go setPlayerZoneIfAbsentScript 的 EXISTS KEYS[2] 同一判据:键存在即封锁)
if KEYS[4] and redis.call('EXISTS', KEYS[4]) == 1 then
  return {3}
end
local cur = redis.call('GET', KEYS[3])
local curHome = redis.call('GET', KEYS[1])
local curPlc = redis.call('GET', KEYS[2])
if cur then
  if not valid_rev(cur) then
    return redis.error_reply('ERR route publish: malformed maprev')
  end
  if rev_ge(cur, rev) then
    -- 同值写回:让本连接随后的 WAITAOF 覆盖「此刻已在缓存里的状态」,
    -- 否则幂等重发(上次的 WAITAOF 超时)会在没确认落盘的情况下回成功。
    redis.call('SET', KEYS[3], cur)
    return {0, cur, curHome or '', curPlc or ''}
  end
else
  -- 无 maprev:这两把键要么从没写过,要么是 P1 导入之前、或绕过本脚本的写者(第 1a 批的 merge_zone)
  -- 留下的遗留值。遗留值存在且与本次不同时一律不覆盖 —— 静默改回库里的旧值就是写错库。
  if (curHome and curHome ~= ARGV[2]) or (curPlc and curPlc ~= ARGV[3]) then
    return {2, '', curHome or '', curPlc or ''}
  end
end
if ARGV[2] == '' then redis.call('DEL', KEYS[1]) else redis.call('SET', KEYS[1], ARGV[2]) end
if ARGV[3] == '' then redis.call('DEL', KEYS[2]) else redis.call('SET', KEYS[2], ARGV[3]) end
-- maprev 最后写,表示「三键已到这一版」。Redis 7 对没有 shebang 的脚本只在第一条写命令前做一次 OOM 检查,
-- 之后不会中途拒写,所以不存在 OOM 半写;顺序只为可读。
redis.call('SET', KEYS[3], rev)
return {1, rev}
`

// PublishOutcome 是一次发布脚本调用的结局。
type PublishOutcome uint8

const (
	// PublishApplied 脚本回 {1, rev}:三键已写成本次的值。
	PublishApplied PublishOutcome = iota + 1
	// PublishSuperseded 脚本回 {0, cur, h, p} 且 cur > rev:缓存已是更新的版本,本次不写。
	// 是否算成功由写者再看 CurHome(第 1a 批 home 写定后不再改,cur 更新而 home 不同只可能是库被回档
	// 或有写者绕过了本脚本,写者必须 fail-closed)。
	PublishSuperseded
	// PublishConfirmed 脚本回 {0, cur, h, p} 且 cur == rev、值与本次相同:幂等重发,已确认。
	PublishConfirmed
	// PublishConflict 脚本回 {0, cur, h, p} 且 cur == rev、值与本次不同:有绕过本脚本的写者,
	// 或库回档后 rev 被复用。写者按失败返回。
	PublishConflict
	// PublishLegacyConflict 脚本回 {2, '', h, p}:缓存没有 maprev,而遗留的 zone / placement 与本次不同。
	// 零写入,写者按失败返回(覆盖它就是把缓存改成可能已过时的库值)。
	PublishLegacyConflict
	// PublishFenced 脚本回 {3}:KEYS[4] 合服围栏存在,零写入。写者映射成 ErrZoneMergeInProgress。
	PublishFenced
)

// String 仅供日志与指标 label(低基数,六个固定值),与 data_service_route_publish_total{result} 的取值同名。
func (o PublishOutcome) String() string {
	switch o {
	case PublishApplied:
		return "applied"
	case PublishSuperseded:
		return "superseded"
	case PublishConfirmed:
		return "confirmed"
	case PublishConflict:
		return "conflict"
	case PublishLegacyConflict:
		return "legacy_conflict"
	case PublishFenced:
		return "fenced"
	default:
		return "unknown"
	}
}

// PublishReply 是分类后的脚本回复。CurRev / CurHome / CurPlacement 是脚本执行那一刻缓存里的值
// (Applied 时就是本次写入的值;Fenced 时全空;LegacyConflict 时 CurRev 为空)。
type PublishReply struct {
	Outcome      PublishOutcome
	CurRev       string
	CurHome      string
	CurPlacement string
}

// ErrUnexpectedPublishReply 表示脚本回复不是 PublishRouteScript 能产生的四种形状之一。
// 写者必须 fail-closed(当作发布失败,不重试):形状不对说明脚本被改过、或连的根本不是同一套契约。
var ErrUnexpectedPublishReply = errors.New("placement: unexpected route publish reply")

// ClassifyPublishReply 把 PublishRouteScript 的回复分类。rev / home / plc 是本次发给脚本的 ARGV。
//
// 只接受四种形状(go-redis 把 Lua 表回成 []any,整数是 int64、串是 string):
//   - {int64(1), rev}:第二个元素必须等于 rev;
//   - {int64(0), cur, h, p}:cur 必须规范且 >= rev(> 为 Superseded;== 时按值是否相同分 Confirmed / Conflict);
//   - {int64(2), "", h, p}:且 h != home || p != plc;
//   - {int64(3)}。
//
// 其余一律回 ErrUnexpectedPublishReply,包括「0 回复里 cur < rev」「2 回复里值反而相同」
// 「cur 是 20 位数字但超过 MaxUint64」—— 脚本不可能产生这些状态,出现就是契约被破坏,按 fail-closed 处理。
func ClassifyPublishReply(raw any, rev, home, plc string) (PublishReply, error) {
	arr, ok := raw.([]any)
	if !ok || len(arr) == 0 {
		return PublishReply{}, fmt.Errorf("%w: %T %v", ErrUnexpectedPublishReply, raw, raw)
	}
	code, ok := arr[0].(int64)
	if !ok {
		return PublishReply{}, fmt.Errorf("%w: first element %T %v is not an integer", ErrUnexpectedPublishReply, arr[0], arr[0])
	}
	switch code {
	case 1:
		if len(arr) != 2 {
			return PublishReply{}, fmt.Errorf("%w: applied reply has %d elements, want 2", ErrUnexpectedPublishReply, len(arr))
		}
		got, ok := arr[1].(string)
		if !ok || got != rev {
			return PublishReply{}, fmt.Errorf("%w: applied reply carries rev %v, sent %q", ErrUnexpectedPublishReply, arr[1], rev)
		}
		return PublishReply{Outcome: PublishApplied, CurRev: rev, CurHome: home, CurPlacement: plc}, nil
	case 0:
		cur, h, p, err := publishReplyTail(arr, "unchanged")
		if err != nil {
			return PublishReply{}, err
		}
		cmp, err := CompareRev(cur, rev)
		if err != nil {
			return PublishReply{}, fmt.Errorf("%w: unchanged reply: %w", ErrUnexpectedPublishReply, err)
		}
		reply := PublishReply{CurRev: cur, CurHome: h, CurPlacement: p}
		switch {
		case cmp < 0:
			// 脚本只在 cur >= rev 时回 0;cur < rev 却回 0 说明比较口径被改过。
			return PublishReply{}, fmt.Errorf("%w: unchanged reply with cached rev %s < sent rev %s", ErrUnexpectedPublishReply, cur, rev)
		case cmp > 0:
			reply.Outcome = PublishSuperseded
		case h == home && p == plc:
			reply.Outcome = PublishConfirmed
		default:
			reply.Outcome = PublishConflict
		}
		return reply, nil
	case 2:
		first, h, p, err := publishReplyTail(arr, "legacy")
		if err != nil {
			return PublishReply{}, err
		}
		if first != "" {
			return PublishReply{}, fmt.Errorf("%w: legacy reply carries rev %q, want empty", ErrUnexpectedPublishReply, first)
		}
		if h == home && p == plc {
			// 脚本只在遗留值与本次不同时回 2;值相同却回 2 说明守卫口径被改过。
			return PublishReply{}, fmt.Errorf("%w: legacy reply with values identical to the sent ones", ErrUnexpectedPublishReply)
		}
		return PublishReply{Outcome: PublishLegacyConflict, CurHome: h, CurPlacement: p}, nil
	case 3:
		if len(arr) != 1 {
			return PublishReply{}, fmt.Errorf("%w: fenced reply has %d elements, want 1", ErrUnexpectedPublishReply, len(arr))
		}
		return PublishReply{Outcome: PublishFenced}, nil
	default:
		return PublishReply{}, fmt.Errorf("%w: unknown reply code %d", ErrUnexpectedPublishReply, code)
	}
}

// publishReplyTail 取 0 / 2 回复的后三个元素,三者都必须是串。
func publishReplyTail(arr []any, kind string) (string, string, string, error) {
	if len(arr) != 4 {
		return "", "", "", fmt.Errorf("%w: %s reply has %d elements, want 4", ErrUnexpectedPublishReply, kind, len(arr))
	}
	var out [3]string
	for i := range out {
		s, ok := arr[i+1].(string)
		if !ok {
			return "", "", "", fmt.Errorf("%w: %s reply element %d is %T, want string", ErrUnexpectedPublishReply, kind, i+1, arr[i+1])
		}
		out[i] = s
	}
	return out[0], out[1], out[2], nil
}
